/**
 * @file    custom_small_data_demo.c
 * @brief   Custom small-data passthrough demo (shared by host POSIX + RTOS FreeRTOS).
 *
 * Behavior:
 *   - host (POSIX):  register callback (print only) → start background thread pushing 1Hz to CLOUD
 *                    → interactive menu to select target payload, send a single message
 *   - RTOS:          register callback (print + reply ack) → main loop pushing 1Hz to CLOUD
 *                    → receive push from other payload → reply ack along the same path
 *
 *   Both sides push telemetry to the cloud at 1Hz; host additionally supports interactive send to a specified payload.
 *   host only prints on receiving a push, does not reply ack (avoids infinite ack loop).
 *
 * Wire protocol (V2):
 *   send topic: thing/product/<sn>/events    method: custom_small_data_transmit_req
 *   recv topic: thing/product/<sn>/events    method: custom_small_data_transmit_event
 *
 * Platform split: #if __linux__/__unix__/__APPLE__ → POSIX section; else → RTOS section.
 */

#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "sample_config.h"
#include "sample_register.h"
#include "sample_lifecycle.h"
#include "custom_small_data/custom_small_data_client.h"
#include "custom_small_data/custom_small_data_publisher.h"
#include "custom_small_data/custom_small_data_types.h"
#include "custom_small_data/custom_small_data_topics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * Platform split
 * ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define CSD_PLATFORM_POSIX 1
#else
#  define CSD_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#endif

/* ============================================================
 * Common utilities
 * ============================================================ */
static const char *channel_addr_str(E_ChannelAddress a)
{
    switch (a) {
        case CHANNEL_ADDRESS_PAYLOAD_NO1: return "PAYLOAD_NO1";
        case CHANNEL_ADDRESS_PAYLOAD_NO2: return "PAYLOAD_NO2";
        case CHANNEL_ADDRESS_PAYLOAD_NO3: return "PAYLOAD_NO3";
        case CHANNEL_ADDRESS_RC:          return "RC";
        case CHANNEL_ADDRESS_RC_SLAVE:    return "RC_SLAVE";
        case CHANNEL_ADDRESS_CLOUD:       return "CLOUD";
        default:                          return "UNKNOWN";
    }
}

typedef struct {
    CustomSmallData_Publisher_t *publisher;
} push_ctx_t;

/* ============================================================
 * Receive callback
 *
 * host:  print only, no ack reply (avoids infinite ack loop)
 * RTOS:  print + reply ack along the same path
 * ============================================================ */
static void on_transmit_push(void *user, const T_TransmitEventPush *push)
{
    push_ctx_t *ctx = (push_ctx_t *)user;
    if (!push || !ctx) return;

    LOG_INFO("[CSD-RX] source=%-12s len=%u payload=\"%.*s\"",
             channel_addr_str(push->source),
             (unsigned)push->payload.len,
             (int)push->payload.len,
             (push->payload.len > 0 && push->payload.data != NULL)
                 ? (const char *)push->payload.data : "");

#if !CSD_PLATFORM_POSIX
    /* RTOS: reply ack to the source */
    if (!ctx->publisher) {
        LOG_WARN("[CSD-RX] publisher not ready, skipping ack");
        return;
    }
    static unsigned s_ack_seq = 0;
    char ack[64];
    snprintf(ack, sizeof(ack), "rtos ack #%u", s_ack_seq++);
    T_TransmitRequest req;
    CustomSmallData_InitTransmitRequest(&req);
    req.target       = push->source;
    req.payload.data = (uint8_t *)ack;
    req.payload.len  = strlen(ack) + 1;
    T_ThingModelReturnCode rc = CustomSmallData_PublishTransmit(ctx->publisher, &req);
    if (rc != THING_MODEL_RC_OK) {
        LOG_ERROR("[CSD-ACK] failed rc=%d target=%s", (int)rc, channel_addr_str(req.target));
    } else {
        LOG_INFO("[CSD-ACK] → %-12s payload=\"%s\"", channel_addr_str(req.target), ack);
    }
#endif
}

/* ============================================================
 * Debug packet capture (host only)
 * ============================================================ */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

/* ============================================================
 * Command-line parsing (POSIX only)
 * ============================================================ */
#if CSD_PLATFORM_POSIX
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

static void usage(const char *prog)
{
    printf("Custom small-data passthrough (CustomSmallData) demo — host side\n");
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --sn <SN>, -s   override device SN\n");
    printf("  --debug,   -d   raw capture: print every PUBLISH on thing/product/<sn>/#\n");
    printf("  --help,    -h   show this help and exit\n");
    printf("Run flow:\n");
    printf("  1. connect broker, subscribe events, register receive callback (print only, no ack)\n");
    printf("  2. background thread auto-pushes telemetry to CLOUD at 1Hz\n");
    printf("  3. interactive menu selects target payload -> send a single message\n");
    printf("  4. press Q to quit\n");
    printf("Peer RTOS behavior:\n");
    printf("  1Hz push telemetry to CLOUD + reply ack on receiving a push\n");
}

static void print_menu(void)
{
    printf("=== Custom Small Data Transmit (1Hz telemetry -> CLOUD started) ===\n");
    printf("  [1] PAYLOAD_NO1  [2] PAYLOAD_NO2  [3] PAYLOAD_NO3\n");
    printf("  [4] RC           [5] RC_SLAVE     [6] CLOUD\n");
    printf("  [Q] Quit\n");
    printf("> ");
    fflush(stdout);
}

/* ── 1Hz telemetry thread: continuously push to CLOUD ── */
static void csd_telemetry_thread(void *arg)
{
    CustomSmallData_Publisher_t *publisher = (CustomSmallData_Publisher_t *)arg;
    unsigned seq = 0;
    while (!sample_should_stop()) {
        char msg[64];
        snprintf(msg, sizeof(msg), "host telemetry #%u", seq++);
        T_TransmitRequest req;
        CustomSmallData_InitTransmitRequest(&req);
        req.target       = CHANNEL_ADDRESS_CLOUD;
        req.payload.data = (uint8_t *)msg;
        req.payload.len  = strlen(msg) + 1;
        T_ThingModelReturnCode rc = CustomSmallData_PublishTransmit(publisher, &req);
        LOG_DEBUG("[CSD-TEL] → CLOUD rc=%d payload=\"%s\"", (int)rc, msg);
        ThingModelOsal_SleepMs(1000);
    }
}
#endif /* CSD_PLATFORM_POSIX */

/* ============================================================
 * Shared initialization
 * ============================================================ */
static int csd_setup(ThingModelClientSession_t *session,
                      CustomSmallData_Client_t **out_client,
                      CustomSmallData_Publisher_t **out_publisher,
                      push_ctx_t *push_ctx)
{
    CustomSmallData_Client_t    *client    = CustomSmallData_CreateClient(session);
    CustomSmallData_Publisher_t *publisher = CustomSmallData_CreatePublisher(session);
    if (!client || !publisher) {
        LOG_ERROR("[CSD] CreateClient / CreatePublisher failed");
        if (client)    CustomSmallData_DestroyClient(client);
        if (publisher) CustomSmallData_DestroyPublisher(publisher);
        return -1;
    }
    push_ctx->publisher = publisher;
    T_ThingModelReturnCode rc =
        CustomSmallData_OnTransmitPush(client, on_transmit_push, push_ctx);
    if (rc != THING_MODEL_RC_OK) {
        LOG_ERROR("[CSD] OnTransmitPush register failed rc=%d", (int)rc);
        CustomSmallData_DestroyPublisher(publisher);
        CustomSmallData_DestroyClient(client);
        return -1;
    }
    *out_client    = client;
    *out_publisher = publisher;
    return 0;
}

static void csd_teardown(CustomSmallData_Client_t *client,
                          CustomSmallData_Publisher_t *publisher)
{
    if (publisher) CustomSmallData_DestroyPublisher(publisher);
    if (client)    CustomSmallData_DestroyClient(client);
}

/* ============================================================
 * Phase A dry-run
 * ============================================================ */
static void phase_a_dry_run(const ThingModelClientSession_Config_t *cfg)
{
    LOG_INFO("=== Phase A: wire topic preview (broker not connected) ===");
    char *evt_topic = CustomSmallData_BuildTopic(
        THING_MODEL_TOPIC_STYLE_V2, cfg->device_sn, THING_MODEL_TOPIC_EVENT);
    LOG_INFO("events topic = %s", evt_topic ? evt_topic : "(null)");
    LOG_INFO("tx method    = custom_small_data_transmit_req     (device → aircraft)");
    LOG_INFO("rx method    = custom_small_data_transmit_event   (aircraft → device)");
    free(evt_topic);
}

/* ============================================================
 * Core run — shared by both ends (Linux + RTOS)
 *
 * The caller guarantees the session has connected successfully. This function only handles:
 * CreateClient/Publisher → register push callback → main loop (Linux runs interactive menu +
 * telemetry thread, RTOS 1Hz main loop) → teardown client/publisher. No Connect / Disconnect.
 *
 * The extra Linux params argc/argv/debug_tap are used for interactive mode;
 * RTOS ignores the argv-related params and enters the 1Hz telemetry loop.
 * ============================================================ */
int custom_small_data_run_with_session(ThingModelClientSession_t *session,
                                       int argc, char **argv, int debug_tap)
{
    (void)argc; (void)argv; (void)debug_tap;
    if (!session) return -1;

#if CSD_PLATFORM_POSIX
    if (debug_tap) {
        ThingModelClientSession_AddListener(session, on_raw_tap, NULL);
        const char *sn = ThingModelClientSession_GetSn(session);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", sn ? sn : "");
        ThingModelClientSession_Subscribe(session, wild, 1);
        LOG_INFO("[debug] raw capture enabled: subscribe %s", wild);
    }
#endif

    push_ctx_t push_ctx = { .publisher = NULL };
    CustomSmallData_Client_t    *client    = NULL;
    CustomSmallData_Publisher_t *publisher = NULL;
    if (csd_setup(session, &client, &publisher, &push_ctx) != 0) {
        return -1;
    }

#if CSD_PLATFORM_POSIX
    /* ── host: 1Hz telemetry thread + interactive menu ── */
    LOG_INFO("[CSD] host mode: 1Hz push to CLOUD + interactive send to a specified payload, print only on receiving a push");

    T_ThingModelThread tel_thr = ThingModelOsal_ThreadCreate(
        csd_telemetry_thread, publisher, "csd_telemetry", 0);
    int tel_started = (tel_thr._h != NULL);
    if (!tel_started) {
        LOG_ERROR("[CSD] ThreadCreate telemetry failed");
    }

    char line[64];
    unsigned send_seq = 0;
    while (!sample_should_stop()) {
        print_menu();
        if (!fgets(line, sizeof(line), stdin)) break;
        char cmd = line[0];
        if (cmd == 'q' || cmd == 'Q') break;
        E_ChannelAddress target;
        switch (cmd) {
            case '1': target = CHANNEL_ADDRESS_PAYLOAD_NO1; break;
            case '2': target = CHANNEL_ADDRESS_PAYLOAD_NO2; break;
            case '3': target = CHANNEL_ADDRESS_PAYLOAD_NO3; break;
            case '4': target = CHANNEL_ADDRESS_RC;          break;
            case '5': target = CHANNEL_ADDRESS_RC_SLAVE;    break;
            case '6': target = CHANNEL_ADDRESS_CLOUD;       break;
            default: continue;
        }
        char msg[80];
        snprintf(msg, sizeof(msg), "host → %s #%u", channel_addr_str(target), send_seq++);
        T_TransmitRequest req;
        CustomSmallData_InitTransmitRequest(&req);
        req.target       = target;
        req.payload.data = (uint8_t *)msg;
        req.payload.len  = strlen(msg) + 1;
        T_ThingModelReturnCode rc = CustomSmallData_PublishTransmit(publisher, &req);
        LOG_INFO("[CSD-TX] → %-12s rc=%d payload=\"%s\"",
                 channel_addr_str(target), (int)rc, msg);
    }

    sample_request_stop();
    if (tel_started) ThingModelOsal_ThreadJoin(tel_thr);
#else
    /* ── RTOS: push telemetry to CLOUD at 1Hz + respond to pushes from other payloads via callback ── */
    LOG_INFO("[CSD] RTOS mode: 1Hz push to CLOUD + respond to pushes from other payloads");
    unsigned seq = 0;
    while (!sample_should_stop()) {
        char msg[64];
        snprintf(msg, sizeof(msg), "rtos telemetry #%u", seq++);
        T_TransmitRequest req;
        CustomSmallData_InitTransmitRequest(&req);
        req.target       = CHANNEL_ADDRESS_CLOUD;
        req.payload.data = (uint8_t *)msg;
        req.payload.len  = strlen(msg) + 1;
        T_ThingModelReturnCode rc = CustomSmallData_PublishTransmit(publisher, &req);
        LOG_DEBUG("[CSD-TEL] → CLOUD rc=%d payload=\"%s\"", (int)rc, msg);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    csd_teardown(client, publisher);
    return 0;
}

/* ============================================================
 * launcher entry —— spin up an independent session then call _run_with_session
 * ============================================================ */
static int custom_small_data_run(const SampleCtx *ctx)
{
    ThingModelClientSession_Config_t cfg = *ctx->cfg;

#if CSD_PLATFORM_POSIX
    int    argc = ctx->argc;
    char **argv = ctx->argv;
    if (has_flag(argc, argv, "--help", "-h")) {
        usage(argv[0]);
        return 0;
    }
    const int debug_tap = has_flag(argc, argv, "--debug", "-d");
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);
    if (sn_override) cfg.device_sn = sn_override;
#else
    const int debug_tap = 0;
    int    argc = 0;
    char **argv = NULL;
#endif
    thing_model_sample_config_print(&cfg);
    phase_a_dry_run(&cfg);

    LOG_INFO("=== Phase B: connect broker ===");
    /* Reuse the launcher's shared session when present and identity is unchanged (no --sn); otherwise
     * open our own raw connection with this sample's bespoke teardown order preserved. */
#if CSD_PLATFORM_POSIX
    bool owned = (ctx->session == NULL) || (sn_override != NULL);
#else
    bool owned = (ctx->session == NULL);
#endif
    ThingModelClientSession_t *session = owned ? ThingModelClientSession_Connect(&cfg) : ctx->session;
    if (!session) {
        LOG_ERROR("session connect failed (broker=%s)", cfg.broker_address);
        return 1;
    }

    int rc = custom_small_data_run_with_session(session, argc, argv, debug_tap);

    /* Disconnect first to stop the network thread, then Destroy client/publisher —
     * the reverse order would let an in-flight msg_cb use an already-freed client → UAF.
     * Only when we own it; the shared session is closed by the launcher. */
    if (owned) ThingModelClientSession_Disconnect(session);
    return rc;
}

SAMPLE_REGISTER(custom_small_data,
                "Custom small-data passthrough: 1Hz push + interactive/response",
                custom_small_data_run);
