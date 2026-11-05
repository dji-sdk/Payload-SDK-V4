/**
 * @file    mop_channel_demo.c
 * @brief   MOP (MQTT-Over-Payload) data channel demo — Linux only.
 *
 * MOP dual-plane architecture:
 *   Control plane (MQTT): Enable/Disable (SERVICE) + ChannelEvent (EVENT push)
 *   Data plane (TCP):  the SDK opens its own TCP connection via the OSAL vtable
 *                      (ThingModelOsal_TcpConnect/Send/Recv/Close) — no direct
 *                      pthread / BSD socket / semaphore calls in this file, so
 *                      the demo compiles cleanly on any platform whose OSAL
 *                      backend implements the tcp_* fields.
 *
 * Flow:
 *   1. Enable → aircraft replies {ip, port, state:"listening"} → connect TCP
 *   2. Wait for ChannelEvent ACTIVE → data plane ready
 *   3. recv thread prints received bytes, send thread sends "mop data seq=N" at 1Hz
 *   4. Ctrl+C → Disable → wait for ChannelEvent CLOSED → close TCP
 *
 * Linux only (aircraft side needs a routable TCP peer). RTOS builds do not include
 * this sample; if they ever do, the FreeRTOS OSAL backend must provide sock_tcp_*.
 *
 * Wire protocol:
 *   Enable  topic: services         method: mop_channel_enable
 *   Enable reply: services_reply    method: mop_channel_enable
 *   Event:        events            method: mop_channel_event
 *   Disable:      services          method: mop_channel_disable
 */

#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "sample_config.h"
#include "sample_register.h"
#include "sample_lifecycle.h"
#include "sample_session.h"
#include "mop_channel/mop_channel_client.h"
#include "mop_channel/mop_channel_types.h"
#include "mop_channel/mop_channel_topics.h"
#include "mop_channel/mop_channel_json.h"
#include "thing_model_client_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================ */
/* Constants                                                     */
/* ============================================================ */

#define DEMO_TIMEOUT_MS    5000u
#define DEFAULT_CHANNEL_UNRELIABLE 49152u
#define DEFAULT_CHANNEL_RELIABLE   49153u
#define CLOSED_WAIT_SEC    5

/* ============================================================ */
/* Global state (shared by callbacks + threads)                  */
/* ============================================================ */

static int               g_sock_fd    = -1;   /* OSAL socket fd */
static int               g_closing_fd = -1;
static T_ThingModelMutex  g_sock_mtx   = { NULL };
static T_ThingModelMutex  g_close_mtx  = { NULL };
static T_ThingModelEvent  g_active_evt = { NULL };
static T_ThingModelEvent  g_closed_evt = { NULL };
static volatile uint16_t  g_current_channel = 0;
static volatile bool      g_got_closed = false;
static E_Role             g_role = ROLE_CLIENT;
static uint32_t           g_channel_id = 0;

/* ============================================================ */
/* Helpers                                                       */
/* ============================================================ */

static const char *role_str(E_Role r)
{
    switch (r) {
        case ROLE_SERVER:  return "SERVER";
        case ROLE_CLIENT:  return "CLIENT";
        default:           return "UNKNOWN";
    }
}

static const char *reliability_str(E_Reliability r)
{
    switch (r) {
        case RELIABILITY_RELIABLE:   return "RELIABLE";
        case RELIABILITY_UNRELIABLE: return "UNRELIABLE";
        default:                     return "UNKNOWN";
    }
}

static const char *state_str(E_ChannelState s)
{
    switch (s) {
        case CHANNEL_STATE_ACTIVE:  return "ACTIVE";
        case CHANNEL_STATE_CLOSED:  return "CLOSED";
        default:                    return "UNKNOWN";
    }
}

static const char *dev_type_str(E_TargetDeviceType t)
{
    switch (t) {
        case TARGET_DEVICE_TYPE_MSDK: return "MSDK";
        case TARGET_DEVICE_TYPE_OSDK: return "OSDK";
        case TARGET_DEVICE_TYPE_PSDK: return "PSDK";
        default:                      return "UNKNOWN";
    }
}

static bool wait_event_sec(T_ThingModelEvent e, int sec)
{
    return ThingModelOsal_EventWait(e, (uint32_t)sec * 1000u) == THING_MODEL_RC_OK;
}

/* ============================================================ */
/* TCP connection                                                */
/* ============================================================ */

static int tcp_connect(const char *ip, int port)
{
    if (!ip || !ip[0] || port <= 0) {
        LOG_ERROR("[MOP-TCP] invalid endpoint ip=%s port=%d", ip ? ip : "(null)", port);
        return -1;
    }

    uint32_t addr_v4 = 0;
    if (ThingModelOsal_InetPton(TM_AF_INET, ip, &addr_v4) != 0) {
        LOG_ERROR("[MOP-TCP] invalid IP: %s", ip);
        return -1;
    }

    int attempt = 0;
    while (!sample_should_stop()) {
        int fd = ThingModelOsal_Socket(TM_AF_INET, TM_SOCK_STREAM, 0);
        if (fd < 0) {
            LOG_ERROR("[MOP-TCP] socket() failed");
            ThingModelOsal_SleepMs(1000);
            continue;
        }

        ThingModelOsal_SetNonblock(fd, true);

        T_TmSockAddr sa = { .family = TM_AF_INET, .port = (uint16_t)port, .addr_v4 = addr_v4 };
        int rc = ThingModelOsal_Connect(fd, &sa);
        if (rc == 0) {
            ThingModelOsal_SetNonblock(fd, false);
            LOG_INFO("[MOP-TCP] connected to %s:%d", ip, port);
            return fd;
        }

        T_TmPollFd pfd = { .fd = fd, .events = TM_POLLOUT, .revents = 0 };
        int r = ThingModelOsal_Poll(&pfd, 1, 2000);
        if (r > 0 && (pfd.revents & TM_POLLOUT)) {
            int err = 0;
            size_t errlen = sizeof(err);
            ThingModelOsal_Getsockopt(fd, TM_SOL_SOCKET, TM_SO_ERROR, &err, &errlen);
            if (err == 0) {
                ThingModelOsal_SetNonblock(fd, false);
                LOG_INFO("[MOP-TCP] connected to %s:%d", ip, port);
                return fd;
            }
        }

        ThingModelOsal_Close(fd);
        attempt++;
        LOG_WARN("[MOP-TCP] connect retry %d...", attempt);
        for (int j = 0; j < 10 && !sample_should_stop(); j++)
            ThingModelOsal_SleepMs(100);
    }

    LOG_WARN("[MOP-TCP] cancelled");
    return -1;
}

static void tcp_shutdown(void)
{
    int fd = -1;
    ThingModelOsal_MutexLock(g_close_mtx);
    ThingModelOsal_MutexLock(g_sock_mtx);
    if (g_sock_fd >= 0) {
        fd = g_sock_fd;
        g_sock_fd = -1;
        g_closing_fd = fd;
    }
    ThingModelOsal_MutexUnlock(g_sock_mtx);

    if (fd >= 0) {
        ThingModelOsal_Shutdown(fd, TM_SHUT_RDWR);
    }
    ThingModelOsal_MutexUnlock(g_close_mtx);
}

static void tcp_close(void)
{
    int fd = -1;
    ThingModelOsal_MutexLock(g_close_mtx);
    ThingModelOsal_MutexLock(g_sock_mtx);
    if (g_closing_fd >= 0) {
        fd = g_closing_fd;
        g_closing_fd = -1;
    }
    ThingModelOsal_MutexUnlock(g_sock_mtx);

    if (fd >= 0) {
        ThingModelOsal_Close(fd);
    }
    ThingModelOsal_MutexUnlock(g_close_mtx);
}

/* ============================================================ */
/* ChannelEvent callback (fired on transport thread, must not call sync API) */
/* ============================================================ */

static void on_channel_event(void *user, const T_ChannelEventPush *push)
{
    (void)user;
    if (!push) return;

    LOG_INFO("[MOP-EVT] channel=%u state=%s", push->channel_id, state_str(push->state));

    if (push->state == CHANNEL_STATE_ACTIVE) {
        ThingModelOsal_EventSignal(g_active_evt);
    } else if (push->state == CHANNEL_STATE_CLOSED) {
        g_got_closed = true;
        tcp_shutdown();
        ThingModelOsal_EventSignal(g_closed_evt);
    }
}

/* ============================================================ */
/* TCP send/recv threads                                         */
/* ============================================================ */

static void recv_thread_fn(void *arg)
{
    (void)arg;
    uint8_t buf[4096];
    while (!sample_should_stop()) {
        ThingModelOsal_MutexLock(g_sock_mtx);
        int fd = g_sock_fd;
        ThingModelOsal_MutexUnlock(g_sock_mtx);
        if (fd < 0) {
            ThingModelOsal_SleepMs(100);
            continue;
        }

        T_TmPollFd pfd = { .fd = fd, .events = TM_POLLIN, .revents = 0 };
        int r = ThingModelOsal_Poll(&pfd, 1, 500);

        if (r == 0) continue;
        if (r < 0) break;

        ssize_t n = ThingModelOsal_Recv(fd, buf, sizeof(buf), 0);

        if (n <= 0) {
            if (!sample_should_stop())
                LOG_WARN("[MOP-RECV] recv returned %zd, socket may be closed", n);
            break;
        }
        bool printable = true;
        for (ssize_t i = 0; i < n && i < 64; i++) {
            if (buf[i] < 0x20 && buf[i] != '\r' && buf[i] != '\n' && buf[i] != '\t') {
                printable = false;
                break;
            }
        }
        if (printable) {
            LOG_INFO("[MOP-RECV] %zd bytes: %.*s", n, (int)(n > 64 ? 64 : n), buf);
        } else {
            char hexbuf[128];
            int off = snprintf(hexbuf, sizeof(hexbuf), "[MOP-RECV] %zd bytes (hex): ", n);
            for (ssize_t i = 0; i < n && i < 32; i++)
                off += snprintf(hexbuf + off, sizeof(hexbuf) - off, "%02x ", buf[i]);
            LOG_INFO("%s", hexbuf);
        }
    }
}

static void send_thread_fn(void *arg)
{
    (void)arg;
    unsigned seq = 0;
    const char *role_label = (g_role == ROLE_SERVER) ? "server" : "client";
    while (!sample_should_stop()) {
        ThingModelOsal_MutexLock(g_sock_mtx);
        int fd = g_sock_fd;
        ThingModelOsal_MutexUnlock(g_sock_mtx);
        if (fd < 0) {
            ThingModelOsal_SleepMs(100);
            continue;
        }

        char msg[128];
        int len = snprintf(msg, sizeof(msg),
            "[payload sdk iot] from=%s channel=%u seq=%u\n",
            role_label, g_channel_id, seq++);
        ssize_t n = ThingModelOsal_Send(fd, msg, (size_t)len, TM_MSG_NOSIGNAL);
        if (n <= 0) {
            LOG_ERROR("[MOP-SEND] send failed (rc=%zd)", n);
            break;
        }
        LOG_INFO("[MOP-SEND] %zd bytes: %s", n, msg);
        ThingModelOsal_SleepMs(1000);
    }
}

/* ============================================================ */
/* Raw packet capture                                            */
/* ============================================================ */

static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

/* ============================================================ */
/* CLI                                                           */
/* ============================================================ */

static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}

static const char *flag_value(int argc, char **argv,
                               const char *lng, const char *shrt, const char *def)
{
    for (int i = 1; i < argc - 1; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return argv[i + 1];
    return def;
}

static int flag_given(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}

/* ============================================================ */
/* Interactive submenu                                           */
/* ============================================================ */

/* Returns 0=success, -1=user cancelled */
static int interactive_menu(E_Role *role, E_Reliability *rel,
                            uint32_t *channel_id,
                            E_TargetDeviceType *target_type,
                            uint32_t *target_addr)
{
    char line[64];

    printf("\n=== MOP mode selection ===\n");
    printf("  1) client + reliable     (active connect, reliable transport, ch=%u)\n", DEFAULT_CHANNEL_RELIABLE);
    printf("  2) client + unreliable   (active connect, unreliable transport, ch=%u)\n", DEFAULT_CHANNEL_UNRELIABLE);
    printf("  3) server + reliable     (wait for connection, reliable transport, ch=%u)\n", DEFAULT_CHANNEL_RELIABLE);
    printf("  4) server + unreliable   (wait for connection, unreliable transport, ch=%u)\n", DEFAULT_CHANNEL_UNRELIABLE);
    printf("  q) back to main menu\n");
    printf("select> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) return -1;
    char cmd = line[0];
    switch (cmd) {
        case '1': *role = ROLE_CLIENT; *rel = RELIABILITY_RELIABLE;   *channel_id = DEFAULT_CHANNEL_RELIABLE;   break;
        case '2': *role = ROLE_CLIENT; *rel = RELIABILITY_UNRELIABLE; *channel_id = DEFAULT_CHANNEL_UNRELIABLE; break;
        case '3': *role = ROLE_SERVER; *rel = RELIABILITY_RELIABLE;   *channel_id = DEFAULT_CHANNEL_RELIABLE;   break;
        case '4': *role = ROLE_SERVER; *rel = RELIABILITY_UNRELIABLE; *channel_id = DEFAULT_CHANNEL_UNRELIABLE; break;
        case 'q': case 'Q': return -1;
        default:  printf("[MOP] invalid selection\n"); return -1;
    }

    /* channel_id */
    printf("channel ID [49152~65535] (Enter for default %u)> ", *channel_id);
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) return -1;
    if (line[0] != '\n') {
        unsigned v = (unsigned)strtoul(line, NULL, 10);
        if (v >= 49152 && v <= 65535) *channel_id = v;
        else printf("[MOP] invalid, using default %u\n", *channel_id);
    }

    /* client mode selects target */
    if (*role == ROLE_CLIENT) {
        printf("target device SDK type [1=MSDK 2=OSDK 3=PSDK] (Enter for default PSDK)> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) return -1;
        if (line[0] != '\n') {
            switch (line[0]) {
                case '1': *target_type = TARGET_DEVICE_TYPE_MSDK; break;
                case '2': *target_type = TARGET_DEVICE_TYPE_OSDK; break;
                case '3': *target_type = TARGET_DEVICE_TYPE_PSDK; break;
                default:  printf("[MOP] invalid, using default PSDK\n"); break;
            }
        }

        printf("target payload position [1|2|3] (Enter for default %u)> ", *target_addr);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) return -1;
        if (line[0] != '\n') {
            unsigned v = (unsigned)strtoul(line, NULL, 10);
            if (v >= 1 && v <= 3) *target_addr = v;
            else printf("[MOP] invalid, using default %u\n", *target_addr);
        }
    }

    return 0;
}

static void usage(const char *prog)
{
    printf(
        "MOP (MQTT-Over-Payload) data channel demo — Linux only\n\n"
        "Usage: %s [options]\n\n"
        "Options:\n"
        "  --role client|server              role (default client)\n"
        "  --reliability reliable|unreliable reliability (default reliable)\n"
        "  --channel <id>                    channel ID 49152~65535 (unspecified: reliable=49153, unreliable=49152)\n"
        "  --target-type msdk|osdk|psdk      target device SDK type (default psdk)\n"
        "  --target <1|2|3>                  target payload position (client mode, default 1)\n"
        "  --observe <sec>                   data send/recv window seconds (0=infinite, Ctrl-C to exit)\n"
        "  --sn <SN>, -s                     override device SN\n"
        "  --debug, -d                       raw packet capture\n"
        "  --help, -h                        help\n\n"
        "Flow:\n"
        "  Phase A: print wire JSON / topic (no broker connection)\n"
        "  Phase B: Enable → connect TCP → ACTIVE → recv/send → Disable → CLOSED\n",
        prog);
}

/* ============================================================ */
/* Phase A: dry-run                                             */
/* ============================================================ */

static void phase_a(const ThingModelClientSession_Config_t *cfg)
{
    LOG_INFO("=== Phase A: wire topic preview (no broker connection) ===");

    T_MopEnableRequest ereq;
    MopChannel_InitMopEnableRequest(&ereq);
    ereq.role              = ROLE_CLIENT;
    ereq.reliability       = RELIABILITY_RELIABLE;
    ereq.channel_id        = DEFAULT_CHANNEL_RELIABLE;
    ereq.target_device_type = TARGET_DEVICE_TYPE_PSDK;
    ereq.target_address    = 1;
    char *p = MopChannel_BuildEnablePayload(THING_MODEL_TOPIC_STYLE_V2, 1, &ereq);
    LOG_INFO("enable  payload = %s", p ? p : "(null)");
    free(p);

    T_MopDisableRequest dreq;
    MopChannel_InitMopDisableRequest(&dreq);
    dreq.channel_id = DEFAULT_CHANNEL_RELIABLE;
    p = MopChannel_BuildDisablePayload(THING_MODEL_TOPIC_STYLE_V2, 2, &dreq);
    LOG_INFO("disable payload = %s", p ? p : "(null)");
    free(p);

    char *t = MopChannel_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg->device_sn, THING_MODEL_TOPIC_SERVICE);
    LOG_INFO("service topic   = %s", t ? t : "(null)");
    free(t);
    t = MopChannel_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg->device_sn, THING_MODEL_TOPIC_EVENT);
    LOG_INFO("events topic    = %s", t ? t : "(null)");
    free(t);

    LOG_INFO("wire methods:");
    LOG_INFO("enable  = mop_channel_%s", MOP_CHANNEL_METHOD_ENABLE);
    LOG_INFO("disable = mop_channel_%s", MOP_CHANNEL_METHOD_DISABLE);
    LOG_INFO("event   = mop_channel_%s", MOP_CHANNEL_METHOD_CHANNEL_EVENT);
}

/* ============================================================ */
/* Phase B: connect broker, run MOP channel                     */
/* ============================================================ */

static int mop_channel_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;
    int    ret  = 0;

    if (has_flag(argc, argv, "--help", "-h")) { usage(argv[0]); return 0; }

    /* ── Default parameters ── */
    E_Role             role         = ROLE_CLIENT;
    E_Reliability      rel          = RELIABILITY_RELIABLE;
    uint32_t           channel_id   = DEFAULT_CHANNEL_RELIABLE;
    E_TargetDeviceType target_type  = TARGET_DEVICE_TYPE_PSDK;
    uint32_t           target_addr  = 1;
    int                observe_sec  = 0;
    int                debug_tap    = has_flag(argc, argv, "--debug", "-d");

    /* ── CLI flags take priority; without flags, use interactive submenu ── */
    if (flag_given(argc, argv, "--role", NULL) || flag_given(argc, argv, "--reliability", NULL)) {
        const char *role_s = flag_value(argc, argv, "--role",        NULL, "client");
        const char *rel_s  = flag_value(argc, argv, "--reliability", NULL, "reliable");
        const char *ch_s   = flag_value(argc, argv, "--channel",     NULL, NULL);
        const char *tgt_s  = flag_value(argc, argv, "--target",      NULL, "1");
        const char *tgt_t  = flag_value(argc, argv, "--target-type", NULL, "psdk");
        observe_sec        = atoi(flag_value(argc, argv, "--observe", NULL, "0"));

        role = (strcmp(role_s, "server") == 0) ? ROLE_SERVER : ROLE_CLIENT;
        rel  = (strcmp(rel_s, "unreliable") == 0) ? RELIABILITY_UNRELIABLE : RELIABILITY_RELIABLE;
        /* When CLI --channel is not specified, pick the default channel based on reliability */
        if (ch_s) {
            channel_id = (uint32_t)strtoul(ch_s, NULL, 10);
        } else {
            channel_id = (rel == RELIABILITY_RELIABLE) ? DEFAULT_CHANNEL_RELIABLE : DEFAULT_CHANNEL_UNRELIABLE;
        }
        if (channel_id < 49152 || channel_id > 65535) {
            uint32_t dflt = (rel == RELIABILITY_RELIABLE) ? DEFAULT_CHANNEL_RELIABLE : DEFAULT_CHANNEL_UNRELIABLE;
            LOG_WARN("[MOP] channel_id %u out of range [49152,65535], using default %u",
                   channel_id, dflt);
            channel_id = dflt;
        }
        if      (strcmp(tgt_t, "msdk") == 0) target_type = TARGET_DEVICE_TYPE_MSDK;
        else if (strcmp(tgt_t, "osdk") == 0) target_type = TARGET_DEVICE_TYPE_OSDK;
        target_addr = (uint32_t)strtoul(tgt_s, NULL, 10);
    } else {
        if (interactive_menu(&role, &rel, &channel_id, &target_type, &target_addr) != 0) {
            LOG_INFO("[MOP] user cancelled");
            return 0;
        }
    }

    g_role       = role;
    g_channel_id = channel_id;

    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);
    if (sn_override) cfg.device_sn = sn_override;
    thing_model_sample_config_print(&cfg);

    /* ── Phase A ── */
    phase_a(&cfg);

    /* ── Phase B ── */
    LOG_INFO("\n=== Phase B: connect broker ===");
    /* Reuse the launcher's shared session when identity matches (owned=false); open a private
     * session only for a distinct --sn. Opening a second session under the SAME client_id as the
     * shared session makes the broker evict one for the other, triggering an endless two-phase
     * re-auth ping-pong between the two sessions. */
    bool owned = false;
    ThingModelClientSession_t *session = sample_session_acquire(ctx, &cfg, &owned);
    if (!session) {
        LOG_ERROR("[MOP] session connect failed (broker=%s)", cfg.broker_address);
        return 1;
    }

    int32_t tap_handle = -1;
    char    tap_topic[160] = {0};
    if (debug_tap) {
        tap_handle = ThingModelClientSession_AddListener(session, on_raw_tap, NULL);
        snprintf(tap_topic, sizeof(tap_topic), "thing/product/%s/#", cfg.device_sn);
        ThingModelClientSession_Subscribe(session, tap_topic, 0);
        LOG_INFO("[MOP] raw tap: subscribe %s", tap_topic);
    }

    MopChannel_Client_t *client = MopChannel_CreateClient(session);
    if (!client) {
        LOG_ERROR("[MOP] CreateClient failed");
        if (tap_handle >= 0) {
            ThingModelClientSession_RemoveListener(session, tap_handle);
            ThingModelClientSession_Unsubscribe(session, tap_topic);
        }
        sample_session_release(session, owned);
        return 1;
    }

    g_sock_mtx   = ThingModelOsal_MutexCreate();
    g_close_mtx  = ThingModelOsal_MutexCreate();
    g_active_evt = ThingModelOsal_EventCreate();
    g_closed_evt = ThingModelOsal_EventCreate();
    g_got_closed = false;

    /* ── Register ChannelEvent callback ── */
    T_ThingModelReturnCode rc = MopChannel_OnChannelEvent(client, on_channel_event, NULL);
    if (rc != THING_MODEL_RC_OK) {
        LOG_ERROR("[MOP] OnChannelEvent failed rc=%d", (int)rc);
        ret = 1;
        goto cleanup_session;
    }

    /* Wait for connection + subscription to stabilize */
    LOG_INFO("[MOP] sleep 3s to let connection stabilize");
    ThingModelOsal_SleepMs(3000);

    /* ── Enable ── */
    LOG_INFO("[MOP] Enable: role=%s reliability=%s channel=%u target_type=%s target_addr=%u",
           role_str(role), reliability_str(rel), channel_id, dev_type_str(target_type),
           (role == ROLE_CLIENT) ? target_addr : 0);

    T_MopEnableRequest ereq;
    MopChannel_InitMopEnableRequest(&ereq);
    ereq.role               = role;
    ereq.reliability        = rel;
    ereq.channel_id         = channel_id;
    ereq.target_device_type = target_type;
    ereq.target_address     = (role == ROLE_CLIENT) ? target_addr : 0;

    T_MopEnableReply ereply;
    MopChannel_InitMopEnableReply(&ereply);
    rc = MopChannel_Enable(client, &ereq, &ereply, DEMO_TIMEOUT_MS);
    if (rc != THING_MODEL_RC_OK) {
        LOG_ERROR("[MOP] Enable failed rc=%d", (int)rc);
        ret = 1;
        goto cleanup_session;
    }

    LOG_INFO("[MOP] Enable reply: result=%d msg=%s channel=%u state=%s ip=%s port=%u",
           ereply.result,
           ereply.result_message ? ereply.result_message : "",
           ereply.channel_id,
           ereply.state ? ereply.state : "",
           ereply.ip ? ereply.ip : "",
           ereply.port);

    if (ereply.result != 0 || !ereply.state || strcmp(ereply.state, "listening") != 0) {
        LOG_ERROR("[MOP] Enable rejected (result=%d state=%s)",
               ereply.result, ereply.state ? ereply.state : "");
        MopChannel_FreeMopEnableReply(&ereply);
        ret = 1;
        goto cleanup_session;
    }

    g_current_channel = (uint16_t)ereply.channel_id;

    /* ── connect TCP data plane ── */
    if (!ereply.ip || ereply.ip[0] == '\0' || ereply.port == 0) {
        LOG_ERROR("[MOP] Enable reply missing ip/port, cannot connect TCP");
        MopChannel_FreeMopEnableReply(&ereply);
        ret = 1;
        goto cleanup_session;
    }
    int fd = tcp_connect(ereply.ip, ereply.port);
    MopChannel_FreeMopEnableReply(&ereply);
    if (fd < 0) { ret = 1; goto cleanup_session; }

    ThingModelOsal_MutexLock(g_sock_mtx);
    g_sock_fd = fd;
    ThingModelOsal_MutexUnlock(g_sock_mtx);

    /* ── Start send/recv threads ── */
    T_ThingModelThread recv_thr = { NULL }, send_thr = { NULL };
    int threads_started = 0;

    /* ── Wait for ChannelEvent ACTIVE (wait indefinitely until peer is ready or user cancels) ── */
    LOG_INFO("[MOP] waiting for ChannelEvent ACTIVE (press Q to cancel)...");
    while (!sample_should_stop()) {
        /* 200ms polling window so Ctrl-C is responsive. */
        int r = ThingModelOsal_EventWait(g_active_evt, 200u);
        if (r == THING_MODEL_RC_OK) {
            if (g_got_closed) {
                LOG_WARN("[MOP] channel closed before ACTIVE — target payload not ready?");
                goto do_disable;
            }
            LOG_INFO("[MOP] channel ACTIVE, data plane ready");
            break;
        }
        if (g_got_closed) {
            LOG_WARN("[MOP] channel CLOSED while waiting for ACTIVE — target payload not ready?");
            goto do_disable;
        }
    }
    if (sample_should_stop()) goto do_disable;

    recv_thr = ThingModelOsal_ThreadCreate(recv_thread_fn, NULL, "mop_recv", 0);
    send_thr = ThingModelOsal_ThreadCreate(send_thread_fn, NULL, "mop_send", 0);
    threads_started = 1;

    /* ── Data window: press Q to exit ── */
    if (observe_sec > 0) {
        LOG_INFO("[MOP] data window: %ds", observe_sec);
        for (int i = 0; i < observe_sec && !sample_should_stop(); i++)
            ThingModelOsal_SleepMs(1000);
    } else {
        LOG_INFO("[MOP] data plane running. Press Q to stop.");
        char line[16];
        while (!sample_should_stop()) {
            if (fgets(line, sizeof(line), stdin)) {
                if (line[0] == 'q' || line[0] == 'Q') break;
            }
        }
    }
    sample_request_stop();

do_disable:
    /* ── Disable ── */
    LOG_INFO("[MOP] Disable channel %u...", g_current_channel);
    T_MopDisableRequest dreq;
    MopChannel_InitMopDisableRequest(&dreq);
    dreq.channel_id = g_current_channel;
    T_Placeholder ph;
    memset(&ph, 0, sizeof(ph));
    rc = MopChannel_Disable(client, &dreq, &ph, DEMO_TIMEOUT_MS);
    LOG_INFO("[MOP] Disable rc=%d", (int)rc);

    /* ── Wait for ChannelEvent CLOSED ── */
    LOG_INFO("[MOP] waiting for ChannelEvent CLOSED (%ds)...", CLOSED_WAIT_SEC);
    if (!wait_event_sec(g_closed_evt, CLOSED_WAIT_SEC)) {
        LOG_WARN("[MOP] CLOSED timeout, forcing socket close");
        tcp_shutdown();
    }

    g_current_channel = 0;

    /* ── join threads (only if started) ── */
    if (threads_started) {
        ThingModelOsal_ThreadJoin(recv_thr);
        ThingModelOsal_ThreadJoin(send_thr);
    }
    tcp_close();

cleanup_session:
    MopChannel_DestroyClient(client);
    ThingModelOsal_EventDestroy(g_active_evt); g_active_evt._h = NULL;
    ThingModelOsal_EventDestroy(g_closed_evt); g_closed_evt._h = NULL;
    ThingModelOsal_MutexDestroy(g_close_mtx);  g_close_mtx._h  = NULL;
    ThingModelOsal_MutexDestroy(g_sock_mtx);   g_sock_mtx._h   = NULL;
    if (tap_handle >= 0) {
        ThingModelClientSession_RemoveListener(session, tap_handle);
        ThingModelClientSession_Unsubscribe(session, tap_topic);
    }
    sample_session_release(session, owned);
    return ret;
}

SAMPLE_REGISTER(mop_channel,
                "MOP data channel: enable → TCP send/recv → disable",
                mop_channel_run);
