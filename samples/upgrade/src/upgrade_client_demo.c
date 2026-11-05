/**
 * @file    upgrade_client_demo.c
 * @brief   Firmware upgrade demo (shared by Linux POSIX + RTOS FreeRTOS).
 *
 * The direction is opposite to other caps —— the aircraft drives, the Payload responds passively + actively pushes state:
 *   · 4 commands  aircraft->device  /services        -> impl-side handlers (UpgradeImpl_On*)
 *   · state push  device->aircraft  /events          -> publisher side (Upgrade_PublishPushUpgradeState)
 *   · firmware transfer  HTTP (GET download / PUT upload)
 *
 * Full upgrade timing:
 *   1. enter_upgrade_mode -> return typed reply: wait_time + firmware HTTP url
 *   2. check_firmware     -> verify the firmware, return result
 *   3. start_upgrade      -> return ok immediately, background thread flashes and pushes state
 *   4. finish_upgrade     -> stop pushing, return ok
 *
 * All platform-specific behavior (firmware storage/verify, HTTP transport, reboot,
 * boot-pending bookkeeping) is behind the upgrade_platform.h domain interface, implemented
 * per platform under projects/<platform>/ and selected at link time. The core state machine
 * below carries no platform #ifdef.
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "sample_device_info.h"
#include "sample_lifecycle.h"           /* sample_should_stop (both platforms) */
#include "upgrade/upgrade_impl.h"       /* UpgradeImpl_On* (4-command responses) */
#include "upgrade/upgrade_publisher.h"  /* Upgrade_PublishPushUpgradeState (state push) */
#include "upgrade/upgrade_types.h"
#include "upgrade/upgrade_topics.h"
#include "common/common_types.h"
#include "upgrade_platform.h"

#include <stdio.h>
#include <string.h>

/* ---- parameters adjustable per deployment environment ---- */
#define UPGRADE_FW_NAME          SAMPLE_DEVICE_NAME "_V" SAMPLE_DEVICE_FIRMWARE_VERSION ".bin"
#define UPGRADE_WAIT_TIME_S      10   /* wait seconds returned to the aircraft */
#define UPGRADE_REBOOT_TIMEOUT_S 12   /* covers bootloader replacement, network and MQTT reconnect */
#define UPGRADE_FINISH_GRACE_S   2    /* after finish, stay silent this long (absorb resends) then wrap up */

/* ---- upgrade runtime state (shared between the transport-thread handler and the background thread) ---- */
static Upgrade_Publisher_t *g_pub          = NULL;
static char                 g_fw_url[160]  = {0};
static volatile int         g_upg_active   = 0;   /* 1=upgrade thread is running and pushing state */
static volatile int         g_cmd_count    = 0;   /* commands received (>0 means upgrade started) */
static void                *g_finish_event = NULL;/* finish_upgrade wakes the COMPLETE loop (OSAL event) */
static void                *g_upg_thread   = NULL;/* background upgrade thread handle (OSAL thread) */
static volatile int         g_upg_started  = 0;
static volatile long        g_last_finish_ts = 0;
static const char          *g_http_iface     = NULL;

static void upg_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }
static long mono_sec(void) { return (long)(ThingModelOsal_TimeMs() / 1000); }

/* progress callback: push UPGRADING state; return non-zero to abort the flash loop. */
static int on_flash_progress(int percent)
{
    if (!g_upg_active) return -1;
    T_UpgradeStatePush p = { .upgrade_stage = UPGRADE_STAGE_UPGRADING, .value = percent };
    Upgrade_PublishPushUpgradeState(g_pub, &p);
    LOG_INFO("[UPG] Flash progress %3d%%", percent);
    return 0;
}

/* --------------------------------------------------------------------------
 * Background upgrade thread: thread_create'd from on_start_upgrade. Pushing from a non-transport
 * thread is safe. The OSAL thread entry signature is void func(void *arg).
 * -------------------------------------------------------------------------- */
static void upgrade_progress_thread(void *arg)
{
    (void)arg;

    /* Stage 1: flash the staged image (POSIX: simulated 0->100; RTOS: no-op, loader does it) */
    LOG_INFO("[UPG] Starting firmware flash...");
    if (upgrade_platform_flash(on_flash_progress) != 0) {
        LOG_WARN("[UPG] Flash stage aborted.");
        g_upg_active = 0;
        return;
    }

    /* commit the boot-pending record (RTOS: write flash marker; POSIX: no-op) */
    if (upgrade_platform_set_boot_pending() != 0) {
        T_UpgradeStatePush failed = { .upgrade_stage = UPGRADE_STAGE_COMPLETE, .value = 2 };
        Upgrade_PublishPushUpgradeState(g_pub, &failed);
        g_upg_active = 0;
        return;
    }

    /* Stage 2: declare reboot, push REBOOTING once */
    LOG_INFO("[UPG] Flash complete, pushing REBOOTING (timeout=%ds)", UPGRADE_REBOOT_TIMEOUT_S);
    T_UpgradeStatePush rebooting = { .upgrade_stage = UPGRADE_STAGE_REBOOTING, .value = UPGRADE_REBOOT_TIMEOUT_S };
    Upgrade_PublishPushUpgradeState(g_pub, &rebooting);
    upg_sleep_ms(1000);

    /* reboot (POSIX: sleep + fall through to COMPLETE loop; RTOS: NVIC_SystemReset, never returns) */
    upgrade_platform_reboot();

    /* Stage 3: push COMPLETE @1Hz until finish_upgrade or aborted (reached only when reboot returns, i.e. POSIX) */
    int result = 1;  /* 1=success 2=failure; demo always succeeds */
    LOG_INFO("[UPG] Pushing COMPLETE(result=%d) @1Hz, waiting for finish_upgrade...", result);
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    while (g_upg_active) {
        T_UpgradeStatePush p = { .upgrade_stage = UPGRADE_STAGE_COMPLETE, .value = result };
        Upgrade_PublishPushUpgradeState(g_pub, &p);
        int rc = (osal && osal->event_wait) ? osal->event_wait(g_finish_event, 1000) : -1;
        if (rc == 0) break;
    }
    LOG_INFO("[UPG] Upgrade thread exiting.");
    g_upg_active = 0;
}

/* pushes COMPLETE @1Hz after the loader has already replaced the image (post-reboot resume) */
static void upgrade_complete_thread(void *arg)
{
    (void)arg;
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    while (g_upg_active) {
        T_UpgradeStatePush complete = { .upgrade_stage = UPGRADE_STAGE_COMPLETE, .value = 1 };
        Upgrade_PublishPushUpgradeState(g_pub, &complete);
        if (osal && osal->event_wait && osal->event_wait(g_finish_event, 1000) == 0) break;
    }
    g_upg_active = 0;
}

/* --------------------------------------------------------------------------
 * impl handlers for the 4 commands (all run on the transport thread: only assign/spawn thread/set flag,
 * do not call sync APIs, do not Destroy).
 * -------------------------------------------------------------------------- */

/* 1) enter_upgrade_mode: typed reply, return wait_time + firmware url.
 *    result_message / url are Freed by the runtime after replying, so heap-allocate them. */
static T_ThingModelReturnCode on_enter_mode(UpgradeImpl_t *impl, void *user,
                                             const T_Placeholder *req,
                                             T_UpgradeEnterModeReply *reply)
{
    (void)impl; (void)user; (void)req;
    g_cmd_count++;
    reply->result         = 0;
    reply->result_message = ThingModelMem_Strdup("ok");
    reply->wait_time      = UPGRADE_WAIT_TIME_S;
    if (upgrade_platform_http_start(UPGRADE_FW_NAME, g_http_iface, g_fw_url, sizeof(g_fw_url)) != 0)
        LOG_WARN("[IMPL enter_upgrade_mode] http start failed");
    reply->url            = ThingModelMem_Strdup(g_fw_url);
    LOG_INFO("[IMPL enter_upgrade_mode] -> wait_time=%d url=%s", reply->wait_time, g_fw_url);
    return THING_MODEL_RC_OK;
}

/* 2) check_firmware: CommonResponse. result_message is char[128], fill with snprintf. */
static T_ThingModelReturnCode on_check_firmware(UpgradeImpl_t *impl, void *user,
                                                 const T_Placeholder *req,
                                                 T_ThingModelCommonReply *reply)
{
    (void)impl; (void)user; (void)req;
    g_cmd_count++;
    long size = 0;
    int ok = upgrade_platform_storage_ok(&size);
    reply->result = ok ? 0 : -1;
    snprintf(reply->result_message, sizeof(reply->result_message),
             ok ? "Firmware check passed (%ld bytes)" : "Firmware not found or empty", size);
    LOG_INFO("[IMPL check_firmware] -> result=%d (%s)", reply->result, reply->result_message);
    return THING_MODEL_RC_OK;
}

/* 3) start_upgrade: CommonResponse. Return ok immediately, then spawn the flash thread. */
static T_ThingModelReturnCode on_start_upgrade(UpgradeImpl_t *impl, void *user,
                                                const T_Placeholder *req,
                                                T_ThingModelCommonReply *reply)
{
    (void)impl; (void)user; (void)req;
    g_cmd_count++;
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "ok");

    if (g_upg_active) {
        LOG_INFO("[IMPL start_upgrade] Upgrade thread already running, ignoring duplicate start.");
        return THING_MODEL_RC_OK;
    }
    if (!upgrade_platform_storage_ok(NULL)) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "firmware not verified");
        return THING_MODEL_RC_OK;
    }
    /* Clear any leftover finish signal, set active, spawn thread. */
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    while (osal && osal->event_wait && osal->event_wait(g_finish_event, 0) == 0) { }
    g_last_finish_ts = 0;
    g_upg_active = 1;
    g_upg_thread = (osal && osal->thread_create)
        ? osal->thread_create(upgrade_progress_thread, NULL, "upgrade_progress", 0) : NULL;
    if (!g_upg_thread) {
        g_upg_active = 0;
        LOG_ERROR("[IMPL start_upgrade] thread_create failed");
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "thread create failed");
    } else {
        LOG_INFO("[IMPL start_upgrade] -> ok, upgrade thread started");
        g_upg_started = 1;
    }
    return THING_MODEL_RC_OK;
}

/* 4) finish_upgrade: CommonResponse. Stop pushing, return ok. */
static T_ThingModelReturnCode on_finish_upgrade(UpgradeImpl_t *impl, void *user,
                                                 const T_Placeholder *req,
                                                 T_ThingModelCommonReply *reply)
{
    (void)impl; (void)user; (void)req;
    g_cmd_count++;
    g_upg_active = 0;     /* let the COMPLETE loop exit */
    if (upgrade_platform_mark_completion_finished() != 0)
        LOG_WARN("[UPG] completion marker clear failed");
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    if (osal && osal->event_signal && g_finish_event)
        osal->event_signal(g_finish_event);
    g_last_finish_ts = mono_sec();
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "ok");
    LOG_INFO("[IMPL finish_upgrade] -> ok, upgrade sequence finished");
    return THING_MODEL_RC_OK;
}

/* ============================================================
 * Core run — shared by both sides (Linux + RTOS)
 * The caller guarantees the session has Connected. Does not Connect / Disconnect.
 * ============================================================ */
int upgrade_run_with_session(ThingModelClientSession_t *session, int observe_sec)
{
    if (!session) return -1;
    const T_ThingModelOsal *osal = ThingModelOsal_Get();

    if (osal && osal->event_create) g_finish_event = osal->event_create();

    const char *sn = ThingModelClientSession_GetSn(session);
    g_pub = Upgrade_CreatePublisher(session);
    UpgradeImpl_t *impl = UpgradeImpl_Create(session, sn);
    UpgradeImpl_OnEnterUpgradeMode(impl, on_enter_mode,     NULL);
    UpgradeImpl_OnCheckFirmware  (impl, on_check_firmware,  NULL);
    UpgradeImpl_OnStartUpgrade   (impl, on_start_upgrade,   NULL);
    UpgradeImpl_OnFinishUpgrade  (impl, on_finish_upgrade,  NULL);
    LOG_INFO("Registered 4 upgrade command handlers, waiting for the aircraft to initiate the upgrade...");

    upgrade_platform_on_session_ready(session, sn);

    /* Resume path: the loader already replaced the image on this boot -> clear the flag and push COMPLETE. */
    if (upgrade_platform_has_pending_completion()) {
        if (upgrade_platform_consume_boot_pending())
            LOG_INFO("[UPG] loader boot-pending flag cleared");
        else
            LOG_WARN("[UPG] loader boot-pending flag clear FAILED (will retry next boot)");
        g_upg_active = 1;
        g_upg_thread = osal && osal->thread_create
            ? osal->thread_create(upgrade_complete_thread, NULL, "upgrade_complete", 0) : NULL;
        g_upg_started = g_upg_thread != NULL;
        LOG_INFO("[UPG] bootloader has replaced the image, starting to push COMPLETE.");
    }

    /* Wait window: the whole upgrade is aircraft-driven. observe_sec only bounds the initial wait
     * while no command has arrived; once g_cmd_count>0 it never times out (a mid-upgrade exit would
     * fail the upgrade). It then exits only on "finish then GRACE seconds of silence" or Ctrl-C. */
    if (observe_sec <= 0)
        LOG_INFO("Waiting indefinitely for the aircraft to initiate the upgrade (auto wrap-up %ds after finish; Ctrl-C to exit)...",
                 UPGRADE_FINISH_GRACE_S);
    else
        LOG_INFO("Waiting up to %ds for the aircraft to initiate the upgrade; once started it no longer times out. Auto wrap-up %ds after finish.",
                 observe_sec, UPGRADE_FINISH_GRACE_S);

    for (int i = 0; !sample_should_stop(); i++) {
        upg_sleep_ms(1000);

        /* (a) finish received and its resends have been silent for GRACE seconds -> round over. */
        long lf = g_last_finish_ts;
        if (lf != 0 && mono_sec() - lf >= UPGRADE_FINISH_GRACE_S) {
            if (upgrade_platform_round_complete()) {
                LOG_INFO("[UPG] Upgrade sequence finished (silent %ds after finish), auto wrap-up and exit.", UPGRADE_FINISH_GRACE_S);
                break;
            }
            LOG_INFO("[UPG] Upgrade sequence finished (silent %ds after finish), resetting state for the next round.", UPGRADE_FINISH_GRACE_S);
            if (g_upg_started && osal && osal->thread_destroy && g_upg_thread) {
                osal->thread_destroy(g_upg_thread);
                g_upg_thread = NULL;
                g_upg_started = 0;
            }
            g_upg_active = 0;
            g_last_finish_ts = 0;
            i = -1;
        }

        /* (b) initial wait timeout: only exit while no command has been received. */
        if (observe_sec > 0 && g_cmd_count == 0 && i + 1 >= observe_sec) {
            LOG_INFO("[UPG] Aircraft did not initiate the upgrade within %ds, exiting.", observe_sec);
            break;
        }
    }

    /* cleanup */
    g_upg_active = 0;
    if (osal && osal->event_signal && g_finish_event)
        osal->event_signal(g_finish_event);
    if (g_upg_started && osal && osal->thread_destroy && g_upg_thread) {
        osal->thread_destroy(g_upg_thread);
        g_upg_thread = NULL;
        g_upg_started = 0;
    }
    UpgradeImpl_Destroy(impl);
    upgrade_platform_on_session_close();
    Upgrade_DestroyPublisher(g_pub);
    g_pub = NULL;
    if (osal && osal->event_destroy && g_finish_event) {
        osal->event_destroy(g_finish_event);
        g_finish_event = NULL;
    }
    upgrade_platform_http_stop();
    return 0;
}

/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback).
 * POSIX-only: this is the CLI/env/dry-run entry model, not a platform behavior split.
 * RTOS drives upgrade_run_with_session directly from launcher_task.c's background-task table.
 * ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include "sample_config.h"
#include "sample_register.h"
#include "sample_lifecycle.h"
#include "sample_session.h"
#include "upgrade/upgrade_client.h"     /* Upgrade_BuildTopic / *Payload (for dry-run) */
#include "upgrade/upgrade_json.h"
#include <stdlib.h>

static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && strcmp(argv[i], lng) == 0) || (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}
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
        "Firmware Upgrade (Upgrade) interface demo — aircraft-driven, Payload responds (impl) + pushes state (publisher) + HTTP firmware service.\n"
        "\nUsage: %s [options]\n\nOptions:\n"
        "  --sn <SN>,-s    override the connected device SN (= MQTT client_id, also the reported device_id)\n"
        "  --iface <if>,-i which interface's IP to use for building the firmware URL (e.g. usb0)\n"
        "  --debug, -d     raw capture: subscribe thing/product/<sn>/# and print every PUBLISH\n"
        "  --help,  -h     show this help and exit\n"
        "\nEnvironment variables:\n"
        "  THING_MODEL_BROKER / _DEVICE_SN / _DEVICE_NAME / _CA_CERT_PATH / _APP_*  connection config\n"
        "  THING_MODEL_HTTP_IFACE    which interface's IP to use for building the firmware URL (same as --iface)\n"
        "  THING_MODEL_OBSERVE_SEC   initial window in seconds to wait for the aircraft (default 0 = wait forever / resident).\n",
        prog);
}

/* --debug raw capture: print every PUBLISH the session receives. */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

static int upgrade_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (has_flag(argc, argv, "--help", "-h")) { usage(argv[0]); return 0; }
    const int debug_tap = has_flag(argc, argv, "--debug", "-d");
    g_http_iface = flag_value(argc, argv, "--iface", "-i",
                              thing_model_sample_getenv("THING_MODEL_HTTP_IFACE", NULL));

    ThingModelOsal_RegisterPosix();
    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);
    if (sn_override) cfg.device_sn = sn_override;
    if (!ctx->background) thing_model_sample_config_print(&cfg);  /* self-check banner: skip for resident services (noise/dupe on the shared session) */

    /* Phase A: dry-run, inspect the wire format (no broker). Skipped for a resident background
     * service — it would just spam the log on every boot. */
    if (!ctx->background) {
    LOG_INFO("=== Phase A: inspect the wire format first (no publish) ===");
    {
        T_UpgradeEnterModeReply er;
        Upgrade_InitUpgradeEnterModeReply(&er);
        er.result = 0;
        er.result_message = (char *)"ok";
        er.wait_time = UPGRADE_WAIT_TIME_S;
        er.url = (char *)"http://<device-ip>:8001/" UPGRADE_FW_NAME;
        char *er_data = UpgradeJson_EncodeUpgradeEnterModeReply(&er);
        LOG_INFO("enter_mode reply data = %s", er_data);
        char *er_wire = UpgradeJson_EncodeServiceRequest(
            1, UPGRADE_TOPIC_CAP_NAME "_" UPGRADE_METHOD_ENTER_UPGRADE_MODE, er_data);
        LOG_INFO("enter_mode reply wire = %s", er_wire);
        free(er_wire); free(er_data);

        T_UpgradeStatePush sp = { .upgrade_stage = UPGRADE_STAGE_UPGRADING, .value = 50 };
        char *sp_data = UpgradeJson_EncodeUpgradeStatePush(&sp);
        LOG_INFO("state push data = %s", sp_data);
        free(sp_data);

        char *t;
        t = Upgrade_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE);
        LOG_INFO("services topic       = %s", t); free(t);
        t = Upgrade_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_EVENT);
        LOG_INFO("events topic         = %s", t); free(t);
    }
    }  /* end !ctx->background */

    /* Phase B: connect, wait for the aircraft to initiate the upgrade. */
    LOG_INFO("=== Phase B: connect to broker, wait for the aircraft to initiate the upgrade ===");

    /* Run on the launcher's shared session when present (resident background service, no reconnect);
     * otherwise open our own (direct/CI run). Network bring-up and boot autostart are the operating
     * system's job (systemd/netplan), not this sample's — see samples/upgrade/deploy/. */
    bool owned = false;
    ThingModelClientSession_t *s = sample_session_acquire(ctx, &cfg, &owned);
    if (!s) {
        LOG_ERROR("session connect failed (broker=%s)", cfg.broker_address);
        return 1;
    }

    if (debug_tap) {
        ThingModelClientSession_AddListener(s, on_raw_tap, NULL);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", cfg.device_sn);
        ThingModelClientSession_Subscribe(s, wild, 0);
        LOG_INFO("[debug] raw capture enabled: subscribe %s", wild);
    }

    const int observe_sec = atoi(thing_model_sample_getenv("THING_MODEL_OBSERVE_SEC", "0"));
    int rc = upgrade_run_with_session(s, observe_sec);

    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER_SERVICE(upgrade, "Firmware upgrade: 4 commands (impl) responses + state push (publisher) + HTTP firmware service", upgrade_run);

#endif /* __linux__ */
