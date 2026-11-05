/**
 * @file    power_client_demo.c
 * @brief   Power management interface demo (arm64 cross-compiled, runs on the payload).
 *
 * Demonstrates the power management interface (apply high power / power-off notification).
 *
 * All third-party deps are vendored under ../../third_party/. Deploy: bin/power_client_demo + lib/*.so.
 *
 * The two operations are initiated in opposite directions, so each uses one side's API:
 *   · power_apply_high_power  device->cloud  → client API: Power_ApplyHighPower
 *   · power_off_notification  cloud->device  → impl  API: PowerImpl_OnPowerOffNotification
 *
 *   Phase A (dry-run, no broker needed):
 *     - SERVICE Build*Payload — wire JSON for power_apply_high_power
 *     - Topic Build           — services / services_reply topic strings
 *     - Reply Encode          — call Encode on a sample PowerOffNotificationReply,
 *                               verify the power-off notification reply data matches the doc byte-for-byte
 *   Phase B (broker required):
 *     1. SERVICE(client) — Power_ApplyHighPower applies for high power, prints reply
 *     2. IMPL(server)    — PowerImpl_OnPowerOffNotification registers a handler,
 *                          enters a wait window; when the cloud sends power_off_notification the handler
 *                          automatically replies preparation_flag=true.
 *
 * Launch arguments:
 *   --debug, -d  raw capture: subscribe thing/product/<sn>/# and print every PUBLISH (for troubleshooting)
 *   --help,  -h  print usage
 *
 * Note: the apply request has no input params (req=NULL → data {}); the power-off notification request also has no input params.
 *    wire method: power_apply_high_power / power_off_notification (cap prefix see proto).
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "power/power_client.h"
#include "power/power_impl.h"
#include "power/power_types.h"
#include "power/power_json.h"
#include "power/power_topics.h"   /* POWER_TOPIC_CAP_NAME / POWER_METHOD_* */
#include "common/common_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================ */
/* Platform split                                                 */
/* ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define POWER_PLATFORM_POSIX 1
#  include "sample_config.h"
#  include "sample_register.h"
#  include "sample_session.h"
#  include "sample_lifecycle.h"
#else
#  define POWER_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#  include "sample_lifecycle.h"   /* sample_should_stop() —— provided on the RTOS side too */
#endif

/* Cross-platform delay: goes through OSAL, consistent behavior on Linux and RTOS. */
static void power_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }

/* IMPL handler: invoked when the cloud sends method "power_off_notification" to /services.
 * Fills reply (result/result_message/preparation_flag), runtime auto-encodes and publishes to
 * /services_reply. user points to a counter, +1 for each message received.
 * Threading model: the callback fires on the transport thread, only does assignments/printf,
 * does not call sync APIs, does not Destroy. */
static T_ThingModelReturnCode on_power_off(PowerImpl_t *impl, void *user,
                                           const T_Placeholder *req,
                                           T_PowerOffNotificationReply *reply)
{
    (void)impl;
    (void)req;
    volatile int *count = user;
    if (count) (*count)++;
    reply->result = 0;
    /* result_message is char * and is freed by the impl runtime, so it must be a heap-allocated string. */
    reply->result_message = ThingModelMem_Strdup("success");
    reply->preparation_flag = true;  /* ready to power off */
    LOG_INFO("[power] power_off_notification received power-off notification -> replying preparation_flag=true");
    return THING_MODEL_RC_OK;
}

/* --debug raw capture: print every PUBLISH the session receives (topic + raw payload). */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

/* Whether the command line carries a given flag (supports both long/short forms). */
static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}

/* Get the value of an option that takes a value: --sn mqtt_a / -s mqtt_a. Returns def if not given (or value missing). */
static const char *flag_value(int argc, char **argv, const char *lng, const char *shrt, const char *def)
{
    for (int i = 1; i < argc - 1; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return argv[i + 1];
    return def;
}

static void usage(const char *prog)
{
    printf(
        "Power management interface demo — apply high power (client) + power-off notification (impl/server).\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --sn <SN>,-s Override the access device SN (= MQTT client_id). Two payloads need different SNs to\n"
        "               connect at the same time (identical client_ids kick each other off). Equivalent to THING_MODEL_DEVICE_SN\n"
        "  --debug, -d  Raw capture: subscribe thing/product/<sn>/# and print every PUBLISH received\n"
        "  --help,  -h  Show this help and exit\n"
        "\n"
        "Run phases:\n"
        "  Phase A  dry-run (no broker): print apply_high_power wire / topic, and call Encode on a sample\n"
        "           power-off notification reply, verify data matches the doc byte-for-byte\n"
        "  Phase B  connect broker: apply for high power; register power-off notification handler and enter wait window\n"
        "\n"
        "Environment variables (all have defaults, override as needed):\n"
        "  THING_MODEL_BROKER        broker address     (default 127.0.0.1)\n"
        "  THING_MODEL_DEVICE_SN     device SN          (default mqtt_test; can be overridden by --sn)\n"
        "  THING_MODEL_CA_CERT_PATH  CA certificate path\n"
        "  THING_MODEL_APP_ID / _APP_NAME / _APP_KEY / _APP_LICENSE   access authentication info\n"
        "  THING_MODEL_OBSERVE_SEC   power-off notification wait window, seconds  (default 15; =0 waits indefinitely)\n"
        "\n"
        "Examples:\n"
        "  %s                         # default: Phase A verify wire + Phase B apply high power/await power-off notification\n"
        "  %s --sn payload_a          # payload A, connect with SN payload_a\n"
        "  %s --sn payload_b          # payload B, start another process with a different SN, both can be online at once\n",
        prog, prog, prog, prog);
}

/* ============================================================
 * Core run — shared by both sides (Linux + RTOS)
 *
 * The caller guarantees the session has Connected successfully. This function only handles: CreateClient →
 * ApplyHighPower → CreateImpl + register power-off notification → observe loop → cleanup.
 * Does not Connect / Disconnect.
 *
 * observe_sec: <=0 means indefinitely, until sample_should_stop() is set.
 * ============================================================ */
int power_run_with_session(ThingModelClientSession_t *session, int observe_sec)
{
    (void)observe_sec;
    if (!session) return -1;

    const char *device_sn = ThingModelClientSession_GetSn(session);

    Power_Client_t *power = Power_CreateClient(session);
    {
        LOG_INFO("[power] ApplyHighPower applying for high power");
        T_ThingModelCommonReply resp = {0};
        T_ThingModelReturnCode rc = Power_ApplyHighPower(power, NULL, &resp, 5000);
        LOG_INFO("[power] ApplyHighPower rc=%d result=%d msg=%s",
               rc, resp.result, resp.result_message[0] ? resp.result_message : "(empty)");
    }

    volatile int off_count = 0;
    PowerImpl_t *impl = PowerImpl_Create(session, device_sn);
    {
        T_ThingModelReturnCode rc = PowerImpl_OnPowerOffNotification(impl, on_power_off, (void *)&off_count);
        LOG_INFO("[power] OnPowerOffNotification registered rc=%d", rc);
    }

    while (!sample_should_stop()) {
        power_sleep_ms(30000);
    }

    PowerImpl_Destroy(impl);
    Power_DestroyClient(power);
    return 0;
}

/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback)
 * ============================================================ */
#if POWER_PLATFORM_POSIX
static int power_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (has_flag(argc, argv, "--help", "-h")) {
        usage(argv[0]);
        return 0;
    }
    const int debug_tap = has_flag(argc, argv, "--debug", "-d");

    ThingModelOsal_RegisterPosix();

    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);
    if (sn_override) cfg.device_sn = sn_override;
    thing_model_sample_config_print(&cfg);

    {
        LOG_INFO("=== Phase A: inspect the wire format first (no publish) ===");

        char *apply = Power_BuildApplyHighPowerPayload(THING_MODEL_TOPIC_STYLE_V2, 1, NULL);
        LOG_INFO("apply_high_power payload = %s", apply);
        free(apply);

        char *svc_topic = Power_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE);
        LOG_INFO("services topic           = %s", svc_topic);
        free(svc_topic);

        char *reply_topic = Power_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE_REPLY);
        LOG_INFO("services_reply topic     = %s", reply_topic);
        free(reply_topic);

        T_PowerOffNotificationReply r;
        Power_InitPowerOffNotificationReply(&r);
        r.result = 0;
        r.result_message = (char *)"success";
        r.preparation_flag = true;
        char *reply_data = PowerJson_EncodePowerOffNotificationReply(&r);
        LOG_INFO("power_off_notification reply data = %s", reply_data);
        char *reply_wire = PowerJson_EncodeServiceRequest(
            1, POWER_TOPIC_CAP_NAME "_" POWER_METHOD_POWER_OFF_NOTIFICATION, reply_data);
        LOG_INFO("power_off_notification reply wire = %s", reply_wire);
        free(reply_wire);
        free(reply_data);
    }

    LOG_INFO("\n=== Phase B: connect broker, call real interfaces ===");
    bool owned = false;
    ThingModelClientSession_t *s = sample_session_acquire(ctx, &cfg, &owned);
    if (!s) {
        LOG_ERROR("session connect failed (broker=%s)", cfg.broker_address);
        LOG_ERROR("hint: Phase A can run without a broker to inspect the JSON");
        return 1;
    }

    if (debug_tap) {
        ThingModelClientSession_AddListener(s, on_raw_tap, NULL);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", cfg.device_sn);
        ThingModelClientSession_Subscribe(s, wild, 0);
        LOG_INFO("\n[debug] raw capture enabled: subscribe %s", wild);
    }

    const int observe_sec = atoi(thing_model_sample_getenv("THING_MODEL_OBSERVE_SEC", "15"));
    int rc = power_run_with_session(s, observe_sec);
    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER(power, "Power management: apply high power (client) + power-off notification (impl)", power_run);
#endif /* POWER_PLATFORM_POSIX */
