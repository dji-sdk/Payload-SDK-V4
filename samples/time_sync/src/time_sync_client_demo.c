/**
 * @file    time_sync_client_demo.c
 * @brief   Time synchronization demo -- shared between Linux POSIX and RTOS FreeRTOS.
 *
 * Time synchronization interface demo. Sets PPS mode and receives UTC time from the aircraft.
 *
 * Third-party deps are all vendored under ../../third_party/ (libmosquitto / libssl /
 * libcrypto prebuilt .so + cJSON source), deployed to the device as: bin/time_sync_client_demo + lib/*.so.
 *
 * Purpose: enter PPS time sync -- the sample sets PPS mode on startup, then fuses "locally captured PPS
 *      rising edges" + "UTC pushed by the aircraft" to compute high-precision local synchronized time,
 *      continuously printing until Ctrl-C.
 *      (GPIO PPS capture is in time_sync_pps.{h,c}; sync computation is in on_time_sync below.)
 *
 * Demonstrates 1 SERVICE + 1 EVENT:
 *   Phase A (dry-run, no broker needed):       [Linux only]
 *     - SERVICE Build*Payload — wire JSON of set_signal_mode(mode=PPS)
 *     - Topic Build           — services / events topic strings
 *     - EVENT Decode          — call Decode on a sample time_sync JSON to verify decoding
 *   Phase B (broker required):
 *     1. EVENT subscribe   — TimeSync_OnSync registers a callback; on UTC, fuse with PPS to compute sync time
 *     2. Set PPS mode  — SetSignalMode(PPS)
 *     3. Run continuously  — receive UTC + output sync time, print cumulative every N seconds, until Ctrl-C
 *
 * Startup args (Linux):
 *   --gpio <N>  GPIO pin number used for PPS capture (default 26; can also use THING_MODEL_PPS_GPIO)
 *   --debug, -d raw capture: subscribe thing/product/<sn>/# and print every PUBLISH
 *   --help, -h  print usage
 *
 * Platform split:
 *   Linux (POSIX): argv/getenv parse --gpio/--debug, Phase A dry-run, Connect +
 *                  time_sync_run_with_session + Disconnect; GPIO PPS capture (time_sync_pps.c)
 *   RTOS:          no argv/env, uses parameter defaults (pps_gpio=0, observe=15s); no PPS GPIO capture,
 *                  only receives and prints UTC; sleep goes through the OSAL vtable.
 *
 * Key abstraction: core logic is concentrated in time_sync_run_with_session(), shared by both sides:
 *   - Linux SAMPLE_REGISTER callback → Connect → time_sync_run_with_session → Disconnect
 *   - RTOS  calls time_sync_run_with_session(shared_session, 0, observe_sec) directly
 *
 * Note: the wire method of set_signal_mode = "time_sync_set_signal_mode";
 *    the event's wire method = "time_sync" (bare name). The naming trick of cap topic_name="time"
 *    is in the header of the time_sync schema.
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "sample_config.h"
#include "sample_register.h"
#include "sample_lifecycle.h"
#include "sample_device_report.h"
#include "sample_session.h"
#include "time_sync/time_sync_client.h"
#include "time_sync/time_sync_types.h"
#include "time_sync/time_sync_json.h"
#include "common/common_types.h"
#include "thing_model_client_log.h"
#include "time_sync_pps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * Platform split
 * ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define TS_PLATFORM_POSIX 1
#  include "sample_config.h"
#  include "sample_register.h"
#  include "sample_lifecycle.h"
#else
#  define TS_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#  include "sample_lifecycle.h"
#endif

/* ============================================================
 * Platform-independent sleep convenience wrapper -- goes through the OSAL vtable (Linux=pthread, RTOS=FreeRTOS)
 * ============================================================ */
static void ts_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }

/* EVENT callback: the aircraft pushes UTC time to /events with method "time_sync".
 * user points to the counter of the current observe window; +1 on each received message, and print the received UTC time.
 *
 * UTC is the wall-clock value at the moment of the "most recent PPS rising edge". If local PPS capture already got a rising edge,
 * fuse the two to compute high-precision local synchronized time:
 *     sync time = pushed UTC + (current monotonic clock - monotonic clock of most recent PPS rising edge)
 * With no PPS rising edge (no hardware / not yet triggered), only print UTC -- graceful degradation.
 *
 * Note: the nano_second field is "nanoseconds" [0,999999999] (see the time_sync schema),
 *    used as the sub-second nanosecond offset.
 *
 * Threading model: the callback fires on the transport thread; do NOT call sync APIs or DestroyClient here; here we only do +1 / printf / read an atomic timestamp, which is safe.
 *
 * The RTOS side has no PPS GPIO capture and only prints UTC; the PPS fusion computation is wrapped in #if POSIX. */
/* Anchor: UTC wall-clock + corresponding PPS local timestamp.
 * Producer = UTC push callback (transport thread).
 * Consumer = tick loop (this task, every 10s).
 *
 * Algorithm:
 *   sync_time = utc_anchor + (now_local_us - pps_at_utc_anchor_us)
 * The UTC callback caches the anchor; the tick uses the anchor to compute the current sync time.
 * Linux uses sysfs GPIO PPS (time_sync_pps.c); RTOS uses BSP EXTI PPS (pps.c). */
static volatile int                       s_anchor_valid = 0;
static volatile T_TimeSyncTimeSyncPush     s_anchor_utc;
static volatile uint64_t                  s_anchor_pps_local_us = 0;

static uint64_t get_local_time_us(void)
{
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    uint64_t ms = (osal && osal->time_ms) ? osal->time_ms() : 0;
    return ms * 1000ULL;
}

static uint64_t get_pps_local_us(void)
{
    return time_sync_pps_last_edge_us();
}

static void on_time_sync(void *user, const T_TimeSyncTimeSyncPush *p)
{
    volatile int *count = user;
    if (!p) return;
    if (count) (*count)++;

    uint64_t pps_us = get_pps_local_us();
    if (pps_us == 0) return;

    s_anchor_utc = *p;
    s_anchor_pps_local_us = pps_us;
    s_anchor_valid = 1;
}

/* Set the time-sync signal mode, print the reply. */
static void set_signal_mode(TimeSync_Client_t *ts, E_TimeSyncSignalMode mode, const char *label)
{
    T_TimeSyncSetSignalModeRequest req;
    TimeSync_InitTimeSyncSetSignalModeRequest(&req);
    req.mode = mode;
    T_ThingModelCommonReply resp = {0};
    T_ThingModelReturnCode rc = TimeSync_SetSignalMode(ts, &req, &resp, 5000);
    LOG_INFO("[time_sync] SetSignalMode(%s) rc=%d result=%d msg=%s",
           label, rc, resp.result, resp.result_message);
}

#if TS_PLATFORM_POSIX
#define TIME_SYNC_PPS_DEFAULT_GPIO 26
/* --debug raw capture: print every PUBLISH received by the session (topic + raw payload).
 * Used to troubleshoot "set_signal_mode timeout / not receiving time_sync" -- to see whether the aircraft
 * replied at all, on which topic, and what the method/seq are. The callback fires on the transport thread; here we only printf, which is safe. */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

/* Whether the command line contains a given flag (supports both long/short forms). */
static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}

/* Read an integer argument of the form "--opt <value>" / "-o <value>"; returns def if absent. */
static int opt_int(int argc, char **argv, const char *lng, const char *shrt, int def)
{
    for (int i = 1; i + 1 < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return atoi(argv[i + 1]);
    return def;
}

/* Print usage: explain the run modes, configurable environment variables, and typical commands. */
static void usage(const char *prog)
{
    printf(
        "Time Sync interface demo — enter PPS mode; use PPS rising edges + aircraft-side UTC to synchronize time.\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --gpio <N>   GPIO pin number used for PPS capture (default %d; can also use THING_MODEL_PPS_GPIO)\n"
        "  --debug, -d  raw capture: subscribe thing/product/<sn>/# and print every PUBLISH received\n"
        "               (use when troubleshooting set_signal_mode timeout / no time_sync received)\n"
        "  --help, -h   show this help and exit\n"
        "\n"
        "Behavior:\n"
        "  On startup the sample sets PPS mode, then continuously receives aircraft-side UTC and outputs sync time until Ctrl-C.\n"
        "  After PPS is set, the aircraft outputs a 1Hz square wave on the GPIO and pushes UTC wall-clock via /events.\n"
        "  This sample captures the GPIO rising edge (monotonic-clock timestamp) and fuses it with the pushed UTC to compute high-precision local sync time:\n"
        "      sync time = pushed UTC + (current monotonic clock - most recent PPS rising-edge monotonic clock)\n"
        "  With no PPS hardware / no permission it degrades automatically: prints UTC only, no sync computation (the receive flow still runs).\n"
        "\n"
        "Run phases:\n"
        "  Phase A  dry-run (no broker): print the set_signal_mode wire / topic, and decode\n"
        "           a sample time_sync JSON to verify encode/decode\n"
        "  Phase B  broker: start PPS capture, subscribe /events, set PPS and continuously output sync time\n"
        "\n"
        "Device identity (device id): use the global parameter --sn, placed before the sample name (applies uniformly to all samples):\n"
        "  sample_things_model --sn <SN> time_sync   # <SN> is the access SN = MQTT client_id = reported device_id\n"
        "  (the env var THING_MODEL_DEVICE_SN also works)\n"
        "\n"
        "Environment variables (all have defaults, override as needed):\n"
        "  THING_MODEL_BROKER        broker address     (default 127.0.0.1)\n"
        "  THING_MODEL_DEVICE_SN     device SN          (default mqtt_test)\n"
        "  THING_MODEL_PPS_GPIO      PPS capture GPIO pin (default %d; --gpio takes precedence)\n"
        "  THING_MODEL_CA_CERT_PATH  CA certificate path (default /path/to/ca.crt)\n"
        "  THING_MODEL_APP_ID / _APP_NAME / _APP_KEY / _APP_LICENSE   access authentication info\n"
        "  THING_MODEL_OBSERVE_SEC   cumulative print interval, seconds (default 15)\n"
        "\n"
        "Examples:\n"
        "  %s                 # enter PPS, continuously output sync time\n"
        "  %s --gpio 17       # PPS signal wired to GPIO 17\n"
        "  THING_MODEL_BROKER=your-broker %s          # specify broker\n"
        "  THING_MODEL_OBSERVE_SEC=5 %s               # change cumulative print interval to 5s\n",
        prog, TIME_SYNC_PPS_DEFAULT_GPIO, TIME_SYNC_PPS_DEFAULT_GPIO,
        prog, prog, prog, prog);
}
#endif /* TS_PLATFORM_POSIX */

/* ============================================================
 * Core run — shared by both sides (Linux + RTOS)
 *
 * Contract: the caller guarantees the session has Connected successfully. This function only handles:
 *   sample_device_report_start → CreateClient → (Linux: PPS capture) →
 *   register UTC callback → SetSignalMode(PPS) → observe loop → cleanup.
 *   It does not do Connect / Disconnect. PPS GPIO capture is Linux-only.
 *
 * pps_gpio:    Linux passes the GPIO pin number; RTOS passes 0 (no capture)
 * observe_sec: observe-window duration (seconds), prints cumulative once per round
 * ============================================================ */
int time_sync_run_with_session(ThingModelClientSession_t *session, int pps_gpio, int observe_sec)
{
    if (!session) return -1;

    const char *sn = ThingModelClientSession_GetSn(session);
    sample_device_report_start(session, sn, NULL);   /* periodically report device_info once connected */

    TimeSync_Client_t *ts = TimeSync_CreateClient(session);

    if (time_sync_pps_start(pps_gpio, NULL, NULL))
        LOG_INFO("[time_sync] PPS capture started (GPIO %d)", pps_gpio);
    else
        LOG_WARN("[time_sync] PPS GPIO %d capture unavailable, degrading to UTC-only print", pps_gpio);

    /* Counter for each observe window. */
    volatile int count = 0;

    /* 1. EVENT subscribe (aircraft → payload pushes UTC time)
      *    Register the callback first; on receiving UTC, fuse with the PPS rising edge to compute sync time (see on_time_sync). */
    {
        LOG_INFO("[time_sync] OnSync registers UTC push callback");
        T_ThingModelReturnCode rc = TimeSync_OnSync(ts, on_time_sync, (void *)&count);
        LOG_INFO("[time_sync] subscribe rc=%d", rc);
    }

    /* 2. Set PPS mode */
    LOG_INFO("[time_sync] SetSignalMode(PPS)");
    set_signal_mode(ts, TIME_SYNC_SIGNAL_MODE_PPS, "PPS");

    /* 3. Run continuously: every 10s, use the cached anchor to compute the current sync time and print.
     *    The callback only caches the anchor without printing (to avoid the high-frequency UTC push flooding the screen).
     *    sync_time = utc_anchor + (now_local_us - pps_at_anchor_us) */
    LOG_INFO("[time_sync] PPS mode set, waiting for UTC push...");
    while (!sample_should_stop()) {
        ts_sleep_ms(10000);
        if (!s_anchor_valid) {
            LOG_INFO("[time_sync] waiting for first UTC push...");
            continue;
        }
        T_TimeSyncTimeSyncPush utc = *(T_TimeSyncTimeSyncPush *)&s_anchor_utc;
        uint64_t anchor_us = s_anchor_pps_local_us;
        uint64_t now_us = get_local_time_us();
        if (now_us < anchor_us) continue;

        uint64_t delta_us = now_us - anchor_us;
        long total_ns  = (long)utc.nano_second + (long)(delta_us % 1000000ULL) * 1000L;
        long carry_sec = total_ns / 1000000000L;
        int  sync_ns   = (int)(total_ns % 1000000000L);

        long total_sec = (long)utc.second + (long)(delta_us / 1000000ULL) + carry_sec;
        int  sync_sec  = (int)(total_sec % 60); long carry_min = total_sec / 60;
        long total_min = (long)utc.minute + carry_min;
        int  sync_min  = (int)(total_min % 60); long carry_hr = total_min / 60;
        int  sync_hour = (int)((utc.hour + carry_hr) % 24);

        LOG_INFO("[time_sync] %04d-%02d-%02d %02d:%02d:%02d.%09d (delta=%lums)",
            utc.year, utc.month, utc.day,
               sync_hour, sync_min, sync_sec, sync_ns,
               (unsigned long)(delta_us / 1000U));
    }

    /* ─── 4. Cleanup ─── */
    time_sync_pps_stop();
    TimeSync_DestroyClient(ts);
    sample_device_report_stop();
    return 0;
}

#if TS_PLATFORM_POSIX
/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback)
 *   - argv parses --gpio / --debug / --help
 *   - Phase A dry-run prints the wire format
 *   - Connect + time_sync_run_with_session + Disconnect
 * ============================================================ */
static int time_sync_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (has_flag(argc, argv, "--help", "-h")) {
        usage(argv[0]);
        return 0;
    }
    /* Raw capture mode: subscribe to the whole SN's # and print every PUBLISH */
    const int debug_tap = has_flag(argc, argv, "--debug", "-d");
    /* PPS capture GPIO pin: --gpio takes precedence, then env var THING_MODEL_PPS_GPIO, then fall back to default. */
    int pps_gpio = atoi(thing_model_sample_getenv("THING_MODEL_PPS_GPIO", "0"));
    if (pps_gpio <= 0) pps_gpio = TIME_SYNC_PPS_DEFAULT_GPIO;
    pps_gpio = opt_int(argc, argv, "--gpio", NULL, pps_gpio);

    /* ─── 1. OSAL registration + Session config (uniformly loaded from dev.env / env vars) ─── */
    ThingModelOsal_RegisterPosix();

    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    thing_model_sample_config_print(&cfg);

    /* ─── 2. Phase A: dry-run, look at the wire format first (runs even without a broker) ─── */
    {
        LOG_INFO("=== Phase A: inspect wire format first (no publish) ===");

        /* SERVICE: set_signal_mode(mode=PPS) */
        T_TimeSyncSetSignalModeRequest req;
        TimeSync_InitTimeSyncSetSignalModeRequest(&req);
        req.mode = TIME_SYNC_SIGNAL_MODE_PPS;
        char *set = TimeSync_BuildSetSignalModePayload(THING_MODEL_TOPIC_STYLE_V2, 1, &req);
        LOG_INFO("set_signal_mode payload = %s", set);
        free(set);

        char *svc_topic = TimeSync_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE);
        LOG_INFO("service topic           = %s", svc_topic);
        free(svc_topic);

        char *evt_topic = TimeSync_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_EVENT);
        LOG_INFO("events  topic           = %s", evt_topic);
        free(evt_topic);

        /* EVENT: call Decode on the doc's sample time_sync data to verify decoding */
        const char *evt_data =
            "{\"year\":2021,\"month\":1,\"day\":1,\"hour\":2,"
            "\"minute\":0,\"second\":0,\"nano_second\":0}";
        T_TimeSyncTimeSyncPush ev;
        if (TimeSyncJson_DecodeTimeSyncTimeSyncPush(evt_data, &ev) == THING_MODEL_RC_OK) {
            LOG_INFO("time_sync decode        = UTC %04d-%02d-%02d %02d:%02d:%02d.%09d",
                   ev.year, ev.month, ev.day, ev.hour, ev.minute, ev.second, ev.nano_second);
        } else {
            LOG_ERROR("time_sync decode        = FAILED");
        }
    }

    /* ─── 3. Phase B: connect to broker, call the real interfaces ─── */
    LOG_INFO("\n=== Phase B: connect to broker, call the real interfaces ===");
    bool owned = false;
    ThingModelClientSession_t *s = sample_session_acquire(ctx, &cfg, &owned);
    if (!s) {
        LOG_ERROR("session connect failed (broker=%s)", cfg.broker_address);
        LOG_ERROR("hint: Phase A can still run without a broker to inspect the JSON");
        return 1;
    }

    /* --debug: install the raw capture + subscribe to the whole SN's #, capturing the aircraft's replies/pushes on any sub-topic.
     * Note: you MUST subscribe to # to receive topics we didn't explicitly subscribe to; otherwise the broker won't deliver them. */
    if (debug_tap) {
        ThingModelClientSession_AddListener(s, on_raw_tap, NULL);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", cfg.device_sn);
        ThingModelClientSession_Subscribe(s, wild, 0);
        LOG_INFO("\n[debug] raw capture on: subscribe %s (every PUBLISH received is printed with [RAW])", wild);
    }

    const int observe_sec = atoi(thing_model_sample_getenv("THING_MODEL_OBSERVE_SEC", "15"));
    int rc = time_sync_run_with_session(s, pps_gpio, observe_sec);

    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER(time_sync, "Time Sync: set PPS mode + fuse PPS/UTC to output local sync time", time_sync_run);
#endif /* TS_PLATFORM_POSIX */
