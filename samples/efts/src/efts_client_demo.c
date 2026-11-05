/**
 * @file    efts_client_demo.c
 * @brief   sample_things_model/efts — arm64 cross-compiled demo, runs on the flight side.
 *
 * EFTS (Emergency Flight Termination System) PWM trigger interface demo.
 * Protocol source: EFTS feature doc (select PWM trigger / query FTS PWM trigger status).
 *
 * All third-party deps are vendored under ../../third_party/ (libmosquitto / libssl /
 * libcrypto prebuilt .so + cJSON source), deployed to the device: bin/efts_client_demo + lib/*.so.
 *
 * Demonstrates 2 SERVICEs, two reply shapes:
 *   Phase A (dry-run, no broker needed):
 *     - SERVICE Build*Payload — efts_select_pwm_trigger / efts_get_trigger_status
 *     - Topic Build           — service topic string
 *   Phase B (broker required) — two run modes:
 *     [default] single-round demo, run in order:
 *       1. GetFtsTriggerStatus — first query and print the current status
 *       2. SelectFtsPwmTrigger — enable the PWM trigger port
 *       3. wait 3 seconds
 *       4. GetFtsTriggerStatus — query and print the current status
 *       5. start hardware PWM output — emit a 400Hz/50% square wave on the PWM pin (see efts_pwm.{c,h}; Linux/RPi only)
 *       6. wait 3 seconds (PWM output ongoing)
 *       7. GetFtsTriggerStatus — query the fts status again
 *       8. stop PWM output — enable=0, reclaim the PWM channel
 *     [stress] pass the "stress" argument on the command line (does not emit PWM):
 *       loop running SelectFtsPwmTrigger + GetFtsTriggerStatus, print stats every 20 seconds
 *       (rounds executed / SelectFtsPwmTrigger successes / GetFtsTriggerStatus successes / full-round successes).
 *       Ctrl+C (SIGINT) gracefully stops and prints the final stats.
 *       usage: ./efts_client_demo stress [per-round interval ms, default 0]
 *     [-h/--help] print usage and exit.
 *
 * Note: neither request has input params (data is empty in the doc). When calling, pass req=NULL,
 *    and data on the wire is {}.
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "efts/efts_client.h"
#include "efts/efts_types.h"
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
#  define EFTS_PLATFORM_POSIX 1
#  include "sample_config.h"
#  include "sample_register.h"
#  include "sample_session.h"
#  include "sample_lifecycle.h"
#else
#  define EFTS_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#  include "sample_lifecycle.h"
#endif
#include "efts_pwm.h"

/* Cross-platform delay: goes through OSAL, so Linux and RTOS behave the same. */
static void efts_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }

static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

static const char *fts_status_str(E_EftsFtsStatus s)
{
    switch (s) {
        case EFTS_FTS_STATUS_NOT_TRIGGERED: return "NOT_TRIGGERED";
        case EFTS_FTS_STATUS_TRIGGERED:     return "TRIGGERED";
        default:                            return "UNKNOWN";
    }
}

/* SelectFtsPwmTrigger — enable the PWM trigger port (CommonResponse). Returns whether it succeeded (rc==0 && result==0). */
static bool efts_do_select(Efts_Client_t *efts, bool verbose)
{
    T_ThingModelCommonReply resp = {0};
    T_ThingModelReturnCode rc = Efts_SelectFtsPwmTrigger(efts, NULL, &resp, 5000);
    bool ok = (rc == 0 && resp.result == 0);
    if (verbose) {
        LOG_INFO("\n[SelectFtsPwmTrigger] enable PWM trigger port");
        LOG_INFO("rc=%d  result=%d msg=%s", rc, resp.result, resp.result_message);
    }
    return ok;
}

/* GetFtsTriggerStatus — query trigger status (typed reply). Returns whether it succeeded (rc==0 && result==0). */
static bool efts_do_get(Efts_Client_t *efts, bool verbose, const char *tag)
{
    T_EftsGetFtsTriggerStatusReply r = {0};
    T_ThingModelReturnCode rc = Efts_GetFtsTriggerStatus(efts, NULL, &r, 5000);
    bool ok = (rc == 0 && r.result == 0);
    if (verbose) {
        LOG_INFO("\n[GetFtsTriggerStatus] %s", tag ? tag : "");
        LOG_INFO("rc=%d  result=%d msg=%s", rc, r.result, r.result_message);
        LOG_INFO("fts_status=%d (%s)  fts_pwm_cnt=%d",
               (int)r.fts_status, fts_status_str(r.fts_status), r.fts_pwm_cnt);
    }
    return ok;
}

#if EFTS_PLATFORM_POSIX
#define EFTS_PWM_DEFAULT_GPIO  18
#define EFTS_PWM_DEFAULT_CHIP   0
/* Stress loop: keep running SelectFtsPwmTrigger + GetFtsTriggerStatus, print stats every 20 seconds; Ctrl+C to exit. */
static void efts_stress_loop(Efts_Client_t *efts, unsigned loop_delay_ms)
{
    LOG_INFO("\n=== stress mode: loop SelectFtsPwmTrigger + GetFtsTriggerStatus (Ctrl+C to stop) ===");
    LOG_INFO("per-round interval=%ums, print stats every 20s", loop_delay_ms);

    uint64_t rounds = 0;
    uint64_t sel_total = 0, sel_ok = 0;
    uint64_t get_total = 0, get_ok = 0;
    uint64_t round_ok = 0;            /* full rounds where both SelectFtsPwmTrigger and GetFtsTriggerStatus succeeded */

    uint64_t start_ms = ThingModelOsal_TimeMs();
    uint64_t last_ms  = start_ms;

    while (!sample_should_stop()) {
        bool so = efts_do_select(efts, false);
        sel_total++; if (so) sel_ok++;

        bool go = efts_do_get(efts, false, NULL);
        get_total++; if (go) get_ok++;

        rounds++;
        if (so && go) round_ok++;

        if (loop_delay_ms) ThingModelOsal_SleepMs((uint32_t)loop_delay_ms);

        uint64_t now_ms = ThingModelOsal_TimeMs();
        if (now_ms - last_ms >= 20000u) {
            long elapsed = (long)((now_ms - start_ms) / 1000u);
            LOG_INFO("\n---- [stress stats] running %lds ----", elapsed);
            LOG_INFO("total rounds        : %llu (full-round successes %llu)",
                   (unsigned long long)rounds, (unsigned long long)round_ok);
            LOG_INFO("SelectFtsPwmTrigger : %llu/%llu succeeded",
                   (unsigned long long)sel_ok, (unsigned long long)sel_total);
            LOG_INFO("GetFtsTriggerStatus : %llu/%llu succeeded",
                   (unsigned long long)get_ok, (unsigned long long)get_total);
            last_ms = now_ms;
        }
    }

    long elapsed = (long)((ThingModelOsal_TimeMs() - start_ms) / 1000u);
    LOG_INFO("\n==== [stress finished] total run %lds ====", elapsed);
    LOG_INFO("total rounds        : %llu (full-round successes %llu)",
           (unsigned long long)rounds, (unsigned long long)round_ok);
    LOG_INFO("SelectFtsPwmTrigger : %llu/%llu succeeded",
           (unsigned long long)sel_ok, (unsigned long long)sel_total);
    LOG_INFO("GetFtsTriggerStatus : %llu/%llu succeeded",
           (unsigned long long)get_ok, (unsigned long long)get_total);
}
#endif /* EFTS_PLATFORM_POSIX */

#if EFTS_PLATFORM_POSIX
static void usage(const char *prog)
{
    printf(
        "EFTS (Emergency Flight Termination System) PWM trigger interface demo\n"
        "\n"
        "Usage:\n"
        "  %s [stress [interval_ms]] [--gpio N] [-d|--debug] [-h|--help]\n"
        "\n"
        "Run modes:\n"
        "  (no args)          single-round demo: query status -> SelectFtsPwmTrigger enable -> wait 3s -> query status\n"
        "                     -> start hardware PWM output(400Hz/50%%) -> wait 3s -> query fts status again -> stop PWM\n"
        "  stress             stress mode: loop SelectFtsPwmTrigger + GetFtsTriggerStatus, print stats every 20s,\n"
        "                     Ctrl+C stops and prints the final stats (stress mode does not emit PWM)\n"
        "  stress <interval_ms>  stress mode, sleep the given milliseconds between rounds (default 0 = full speed)\n"
        "  --gpio N           hardware PWM output pin (18/12->channel0, 19/13->channel1; default %d; also via THING_MODEL_EFTS_GPIO)\n"
        "  -d, --debug        raw capture: subscribe thing/product/<sn>/# and print every received PUBLISH\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  %s                 # single-round demo (incl. hardware PWM output)\n"
        "  %s --gpio 19       # hardware PWM output to GPIO 19 (PWM1)\n"
        "  %s stress          # full-speed stress\n"
        "  %s stress 200      # stress with a 200ms interval per round\n"
        "\n"
        "Connection config (may be overridden by environment variables, otherwise built-in defaults are used):\n"
        "  THING_MODEL_APP_ID / APP_NAME / APP_KEY / APP_LICENSE  application credentials\n"
        "  THING_MODEL_DEVICE_SN        source_device_id in the topic\n"
        "  THING_MODEL_BROKER           broker address (TLS:8883 / plain:1883)\n"
        "  THING_MODEL_CA_CERT_PATH     CA certificate path\n"
        "  THING_MODEL_EFTS_GPIO        hardware PWM output pin (default %d; --gpio takes priority)\n"
        "  THING_MODEL_EFTS_PWMCHIP     pwmchip number (default 0; often required on Pi 5, first check ls /sys/class/pwm/)\n"
        "\n"
        "Note: this program is an aarch64 cross-compiled artifact and must be deployed to the flight side to run;\n"
        "    it depends on ../lib/*.so (libmosquitto/libssl/libcrypto),\n"
        "    ensure LD_LIBRARY_PATH points to that directory before running.\n"
        "    PWM uses the Raspberry Pi on-chip hardware PWM (/sys/class/pwm), so add\n"
        "    dtoverlay=pwm-2chan to config.txt and reboot; the pin must be a hardware PWM pin (18/12/19/13).\n",
        prog, EFTS_PWM_DEFAULT_GPIO, prog, prog, prog, prog, EFTS_PWM_DEFAULT_GPIO);
}
#endif /* EFTS_PLATFORM_POSIX */

/* ============================================================
 * Core run_once — shared by both sides (Linux + RTOS)
 *
 * The caller guarantees the session has connected successfully. This function only handles: CreateClient →
 * GetFtsTriggerStatus → SelectFtsPwmTrigger → wait 3 seconds → GetFtsTriggerStatus →
 * (Linux/RPi extra) hardware PWM output demo → GetFtsTriggerStatus → cleanup. It does not Connect / Disconnect.
 *
 * PWM (steps 5~8) is a Raspberry Pi /sys/class/pwm-only peripheral, so the whole segment is guarded by EFTS_PLATFORM_POSIX
 * and is excluded at compile time on the RTOS side (the GD32 has no such peripheral).
 * ============================================================ */
int efts_run_once(ThingModelClientSession_t *session)
{
    if (!session) return -1;

    Efts_Client_t *efts = Efts_CreateClient(session);
    if (!efts) return -1;

    /* 1. query status before enabling */
    efts_do_get(efts, true, "step 1: query current status before SelectFtsPwmTrigger");

    /* 2. SelectFtsPwmTrigger — enable the payload's PWM trigger port */
    efts_do_select(efts, true);

    /* 3. wait 3 seconds */
    LOG_INFO("\n[wait] step 3: sleep 3s");
    efts_sleep_ms(3000);

    /* 4. read status once */
    efts_do_get(efts, true, "step 4: query current status after SelectFtsPwmTrigger");

    /* 5~8. hardware PWM output demo.
     *      On Linux/RPi: sysfs /sys/class/pwm via projects/linux/efts_pwm_linux_sysfs.c
     *      On GD32:      TIMER1 CH2 via bsp/drivers/BSP/pwm.c
     *      When unavailable: print a warning then run the rest of the flow as usual. */
    int pwm_gpio = 0;
#if EFTS_PLATFORM_POSIX
    {
        const char *g = thing_model_sample_getenv("THING_MODEL_EFTS_GPIO", "0");
        int v = g ? atoi(g) : 0;
        pwm_gpio = (v > 0) ? v : EFTS_PWM_DEFAULT_GPIO;
    }
#endif

    LOG_INFO("\n[5] PWM: FTS-triggered hardware PWM output (GPIO %d, %dHz/%d%%)",
           pwm_gpio, EFTS_PWM_FREQ_HZ, EFTS_PWM_DUTY_PCT);
    if (!efts_pwm_start(pwm_gpio))
        LOG_WARN("[PWM] GPIO %d hardware PWM unavailable, skip wave output, rest of the flow continues as usual",
               pwm_gpio);

    /* 6. wait 3 seconds (PWM output ongoing) */
    LOG_INFO("\n[wait] step 6: sleep 3s (PWM output ongoing)");
    efts_sleep_ms(3000);

    /* 7. query the fts status again */
    efts_do_get(efts, true, "step 7: query fts status after PWM output");

    /* 8. stop PWM immediately */
    efts_pwm_stop();

    Efts_DestroyClient(efts);
    return 0;
}

/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback)
 * ============================================================ */
#if EFTS_PLATFORM_POSIX
/* Read an integer argument of the form "--opt <value>"; return def if absent. */
static int opt_int(int argc, char **argv, const char *lng, int def)
{
    for (int i = 1; i + 1 < argc; i++)
        if (lng && strcmp(argv[i], lng) == 0)
            return atoi(argv[i + 1]);
    return def;
}

static int efts_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]);
        return 0;
    }
    bool stress = (argc > 1 && strcmp(argv[1], "stress") == 0);
    unsigned loop_delay_ms = (stress && argc > 2) ? (unsigned)strtoul(argv[2], NULL, 10) : 0;
    bool debug_tap = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_tap = true;
            if (!stress) break;
        }
    }

    /* PWM output pin: --gpio takes priority, then the THING_MODEL_EFTS_GPIO environment variable, then fall back to the default.
     * Write it back to the environment for the PWM segment (#if POSIX) of the shared efts_run_once() to read. */
    {
        int g = atoi(thing_model_sample_getenv("THING_MODEL_EFTS_GPIO", "0"));
        if (g <= 0) g = EFTS_PWM_DEFAULT_GPIO;
        g = opt_int(argc, argv, "--gpio", g);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", g);
        setenv("THING_MODEL_EFTS_GPIO", buf, 1);
    }

    /* Unknown-argument check: only look at the first argument; known first args are stress / -d|--debug / --gpio (with value). */
    if (argc > 1 && !stress) {
        const char *a = argv[1];
        bool known = (strcmp(a, "--debug") == 0 || strcmp(a, "-d") == 0 ||
                      strcmp(a, "--gpio") == 0);
        if (!known) {
            fprintf(stderr, "unknown argument: %s\n\n", argv[1]);
            usage(argv[0]);
            return 2;
        }
    }

    ThingModelOsal_RegisterPosix();

    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    thing_model_sample_config_print(&cfg);

    {
        LOG_INFO("=== Phase A: inspect the wire format first (no publish) ===");

        char *sel = Efts_BuildSelectFtsPwmTriggerPayload(THING_MODEL_TOPIC_STYLE_V2, 1, NULL);
        LOG_INFO("select_fts_pwm_trigger payload = %s", sel);
        free(sel);

        char *get = Efts_BuildGetFtsTriggerStatusPayload(THING_MODEL_TOPIC_STYLE_V2, 1, NULL);
        LOG_INFO("get_fts_trigger_status payload = %s", get);
        free(get);

        char *topic = Efts_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE);
        LOG_INFO("service topic                  = %s", topic);
        free(topic);
    }

    LOG_INFO("\n=== Phase B: connect broker, call the real interfaces ===");
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
        LOG_INFO("[debug] raw capture: subscribe %s\n", wild);
    }

    int rc;
    if (stress) {
        Efts_Client_t *efts = Efts_CreateClient(s);
        efts_stress_loop(efts, loop_delay_ms);
        Efts_DestroyClient(efts);
        rc = 0;
    } else {
        rc = efts_run_once(s);
    }

    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER(efts, "Emergency Flight Termination System (EFTS) PWM trigger: single-round demo / stress test", efts_run);
#endif /* EFTS_PLATFORM_POSIX */
