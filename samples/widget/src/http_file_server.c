/*
 * http_file_server.c — static-file HTTP server implementation (mongoose 7.x, pure C).
 *
 * A background thread (via ThingModelOsal_ThreadCreate) runs mg_mgr_poll; on MG_EV_HTTP_MSG
 * it serves base_dir/<lang> as the static root, with lang resolved dynamically per request
 * via the callback (following the language selected by get_file_list). Read-only static serving.
 *
 * Platform split: mongoose + `stat()` are still POSIX-specific, so this file stays inside
 * `#if __linux__`. Only the pthread bits moved to OSAL so we no longer include <pthread.h>.
 */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include "http_file_server.h"

#include "mongoose.h"

#include "thing_model_client/thing_model_client_osal.h"

#include "thing_model_client_log.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static struct mg_mgr       s_mgr;
static T_ThingModelThread  s_thread = { NULL };
static volatile int        s_running = 0;
static char                s_base_dir[512];
static http_lang_fn        s_lang_fn  = NULL;
static void               *s_lang_user = NULL;

/* HTTP event: on receiving a request, serve the static directory base_dir/<lang>. */
static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        /* The app-side download URL looks like /v1/<file> (DJI widget convention); after stripping the /v1 prefix,
         * serve by web_root. */
        if (hm->uri.len >= 4 && memcmp(hm->uri.buf, "/v1/", 4) == 0) {
            hm->uri.buf += 3;   /* keep the leading '/' */
            hm->uri.len -= 3;
        }

        /* Resolve the root directory by current language = base_dir/<lang> (lang follows get_file_list). */
        const char *lang = s_lang_fn ? s_lang_fn(s_lang_user) : NULL;
        if (!lang || !lang[0]) lang = "en";
        char web_root[600];
        snprintf(web_root, sizeof(web_root), "%s/%s", s_base_dir, lang);

        /* Diagnostic: print the mapped request line + whether the resolved local path exists (to quickly pinpoint 404s). */
        char path[700];
        snprintf(path, sizeof(path), "%s%.*s", web_root,
                 (int)hm->uri.len, hm->uri.buf);
        struct stat st;
        int exists = (stat(path, &st) == 0);
        LOG_INFO("[http] %.*s %.*s -> %s (%s)",
               (int)hm->method.len, hm->method.buf,
               (int)hm->uri.len, hm->uri.buf,
               path, exists ? "FOUND" : "404 MISSING");

        struct mg_http_serve_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.root_dir = web_root;
        mg_http_serve_dir(c, hm, &opts);
    }
}

static void server_loop(void *arg)
{
    (void)arg;
    while (s_running)
        mg_mgr_poll(&s_mgr, 200);
}

int http_file_server_start(const char *listen_url, const char *base_dir,
                           http_lang_fn lang_fn, void *user)
{
    if (s_running) return 0;
    if (!listen_url || !base_dir) return -1;

    snprintf(s_base_dir, sizeof(s_base_dir), "%s", base_dir);
    s_lang_fn   = lang_fn;
    s_lang_user = user;

    mg_log_set(MG_LL_ERROR);   /* silence mongoose's per-connection DEBUG spam; keep only ERROR */
    mg_mgr_init(&s_mgr);
    struct mg_connection *c = mg_http_listen(&s_mgr, listen_url, ev_handler, NULL);
    if (!c) {
        mg_mgr_free(&s_mgr);
        return -1;
    }

    s_running = 1;
    s_thread  = ThingModelOsal_ThreadCreate(server_loop, NULL, "http_file", 0);
    if (!s_thread._h) {
        s_running = 0;
        mg_mgr_free(&s_mgr);
        return -1;
    }
    return 0;
}

void http_file_server_stop(void)
{
    if (!s_running) return;
    s_running = 0;
    ThingModelOsal_ThreadJoin(s_thread);
    s_thread._h = NULL;
    mg_mgr_free(&s_mgr);
}

int http_file_server_is_running(void)
{
    return s_running;
}
#endif /* __linux__ || __unix__ || __APPLE__ */
