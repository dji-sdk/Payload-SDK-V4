/*
 * upgrade_http_server.c — implementation of the HTTP service for upgrade firmware (mongoose 7.x, pure C).
 *
 * A background thread (started via ThingModelOsal_ThreadCreate) runs mg_mgr_poll;
 * on MG_EV_HTTP_MSG it dispatches by method:
 *   · PUT/POST: write the request body to disk at web_root/<file> (aircraft/cloud uploads firmware to Payload)
 *   · others (GET): treat web_root as the static root and return the file (aircraft downloads firmware)
 * Plaintext HTTP only (MG_ENABLE_OPENSSL=0, see the upper-level CMakeLists); firmware transfer runs over
 * the aircraft's local network.
 *
 * Platform split: mongoose + POSIX file I/O + getifaddrs are still Linux-only, so this file
 * stays inside `#if __linux__`. The thread machinery goes through OSAL so the same code shape
 * would work on any backend that provides sock_tcp_listen + threads if we ever swap mongoose out.
 */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include "upgrade_http_server.h"

#include "mongoose.h"

#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static struct mg_mgr       s_mgr;
static T_ThingModelThread  s_thread = { NULL };
static volatile int        s_running = 0;
static char                s_web_root[512];

/* HTTP event handling: PUT/POST write a file, GET returns a static file. */
static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    /* ---- PUT/POST: aircraft/cloud uploads firmware to the Payload ---- */
    if (mg_match(hm->method, mg_str("PUT"), NULL) ||
        mg_match(hm->method, mg_str("POST"), NULL)) {
        /* Strip the leading '/' from the URI to get the file name, then assemble it under web_root and write to disk. */
        const char *uri = hm->uri.buf;
        size_t      uri_len = hm->uri.len;
        if (uri_len > 0 && uri[0] == '/') { uri++; uri_len--; }

        char fpath[600];
        snprintf(fpath, sizeof(fpath), "%s/%.*s", s_web_root, (int)uri_len, uri);

        FILE *f = fopen(fpath, "wb");
        if (!f) {
            LOG_ERROR("[http] PUT %s -> cannot open for writing: %s", fpath, strerror(errno));
            mg_http_reply(c, 500, "", "write error\n");
            return;
        }
        size_t written = fwrite(hm->body.buf, 1, hm->body.len, f);
        fclose(f);
        if (written != hm->body.len) {
            LOG_ERROR("[http] PUT %s -> incomplete write %zu/%zu", fpath, written, hm->body.len);
            mg_http_reply(c, 500, "", "write incomplete\n");
            return;
        }
        LOG_INFO("[http] PUT %s -> saved %zu bytes", fpath, written);
        mg_http_reply(c, 200, "", "ok\n");
        return;
    }

    /* ---- GET: treat web_root as the static root and return the firmware file ---- */
    char path[700];
    snprintf(path, sizeof(path), "%s%.*s", s_web_root, (int)hm->uri.len, hm->uri.buf);
    struct stat st;
    int exists = (stat(path, &st) == 0);
    /* Only log GETs that actually hit the firmware file (upgrade-related). The aircraft may probe
     * paths this service does not provide, such as widget config files → always 404, no logging to
     * avoid flooding the screen (this sample only tests the upgrade feature). */
    if (exists)
        LOG_INFO("[http] GET %.*s -> %s (FOUND)",
                 (int)hm->uri.len, hm->uri.buf, path);

    struct mg_http_serve_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.root_dir = s_web_root;
    mg_http_serve_dir(c, hm, &opts);
}

static void server_loop(void *arg)
{
    (void)arg;
    while (s_running)
        mg_mgr_poll(&s_mgr, 200);
}

int upgrade_http_server_start(const char *listen_url, const char *web_root)
{
    if (s_running) return 0;
    if (!listen_url || !web_root) return -1;

    snprintf(s_web_root, sizeof(s_web_root), "%s", web_root);

    mg_log_set(MG_LL_ERROR);   /* silence mongoose's per-connection DEBUG flooding, keep only ERROR */
    mg_mgr_init(&s_mgr);
    struct mg_connection *c = mg_http_listen(&s_mgr, listen_url, ev_handler, NULL);
    if (!c) {
        mg_mgr_free(&s_mgr);
        return -1;
    }

    s_running = 1;
    s_thread  = ThingModelOsal_ThreadCreate(server_loop, NULL, "upg_http", 0);
    if (!s_thread._h) {
        s_running = 0;
        mg_mgr_free(&s_mgr);
        return -1;
    }
    return 0;
}

void upgrade_http_server_stop(void)
{
    if (!s_running) return;
    s_running = 0;
    ThingModelOsal_ThreadJoin(s_thread);
    s_thread._h = NULL;
    mg_mgr_free(&s_mgr);
}

int upgrade_http_server_is_running(void)
{
    return s_running;
}

int upgrade_http_local_ip(char *out, size_t n, const char *prefer_iface)
{
    if (!out || n == 0) return -1;
    snprintf(out, n, "127.0.0.1");   /* default fallback */

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) return -1;

    /* Three priority tiers, iterated tier by tier, returning as soon as a higher tier hits — the key is to
     * let the aircraft get the Payload's IP on the "interface interconnected with the aircraft", rather than
     * an arbitrary non-loopback interface:
     *   tier 1  exact match of prefer_iface (specified by the user via --iface / THING_MODEL_HTTP_IFACE, e.g. usb0)
     *   tier 2  an interface whose name starts with "usb" (a DJI Payload interconnects with the aircraft over the USB network, firmware goes through this port)
     *   tier 3  any non-loopback IPv4 (fallback, keeps the old behavior, only meaningful for local self-test)
     * P_EXACT/P_USB/P_ANY are the three tiers above. */
    enum { P_EXACT, P_USB, P_ANY, P_COUNT };
    int found = -1;
    for (int pass = (prefer_iface && prefer_iface[0]) ? P_EXACT : P_USB;
         pass < P_COUNT && found != 0; pass++) {
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            /* skip loopback 127.0.0.0/8 */
            if ((ntohl(sa->sin_addr.s_addr) >> 24) == 127) continue;

            if (pass == P_EXACT && strcmp(ifa->ifa_name, prefer_iface) != 0) continue;
            if (pass == P_USB   && strncmp(ifa->ifa_name, "usb", 3) != 0)    continue;

            if (inet_ntop(AF_INET, &sa->sin_addr, out, (socklen_t)n)) {
                found = 0;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return found;
}
#endif /* __linux__ || __unix__ || __APPLE__ */
