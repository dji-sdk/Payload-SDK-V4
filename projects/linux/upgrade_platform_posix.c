/*
 * upgrade_platform_posix.c — Linux implementation of the upgrade_platform.h domain interface.
 *
 * This is where the REAL Raspberry Pi upgrade behavior lives (the shared state machine in
 * samples/upgrade/src/upgrade_client_demo.c stays platform-free):
 *   · storage_ok / http_start  — glob the web root for the ACTUAL delivered firmware (the aircraft
 *     ships a NEW version, so the running binary's own compile-time name must not be hard-coded),
 *     lazy HTTP server (the port is only held while an upgrade is in progress).
 *   · flash        — push progress, then REALLY install: overwrite the running binary
 *     (atomic cp→chmod→mv+sync via upgrade_platform_linux.c).
 *   · set_boot_pending / has_pending_completion / mark_completion_finished — persist the upgrade
 *     state across the reboot (/var/lib/psdk_iot) so the next boot reports COMPLETE.
 *   · reboot       — reboot(2) syscall with command fallbacks (really reboots; needs root).
 *   · round_complete = 0 — stay resident after each round (V3-style always-on service).
 *   · on_session_ready — background widget get_file_list so DA2 lists the payload; config assets
 *     resolved against the binary's own directory (boot cwd independent).
 */
#include "upgrade_platform.h"
#include "upgrade_platform_linux.h"
#include "upgrade_http_server.h"
#include "sample_device_info.h"
#include "sample_config.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "app_widget.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <time.h>

#define UPG_HTTP_PORT_DEFAULT "8001"                /* override via THING_MODEL_UPGRADE_HTTP_PORT (8000 is widget's) */
#define UPG_WEB_ROOT          "/tmp/upgrade_share"  /* firmware directory (GET serves / PUT writes) */

/* ── Apply gate ────────────────────────────────────────────────────────────────────────────────
 * By default the upgrade sample is STAGE-ONLY: it drives the full protocol (respond to the 4
 * commands, serve the firmware, push state) but never overwrites the running binary and never
 * reboots the host. Really installing + rebooting is an opt-in DEPLOYMENT integration, gated twice:
 *   1) compile: -DUPGRADE_REAL_INSTALL=ON  — without it, no install/reboot code is even built in.
 *   2) runtime: env PSDK_UPGRADE_APPLY      — "1"/"real" = install + reboot;
 *                                             "dry" = install to a temp path but do NOT reboot;
 *                                             unset/other = stage-only.
 * See samples/upgrade/deploy/ for the systemd unit + installer that arm both gates on a real rig. */
typedef enum { UPG_APPLY_STAGE = 0, UPG_APPLY_DRY = 1, UPG_APPLY_REAL = 2 } E_UpgApplyMode;

static E_UpgApplyMode upg_apply_mode(void)
{
#ifdef UPGRADE_REAL_INSTALL
    const char *v = thing_model_sample_getenv("PSDK_UPGRADE_APPLY", NULL);
    if (v && (!strcmp(v, "1") || !strcmp(v, "real"))) return UPG_APPLY_REAL;
    if (v && !strcmp(v, "dry"))                        return UPG_APPLY_DRY;
#endif
    return UPG_APPLY_STAGE;
}

static WidgetImpl_t *s_wimpl = NULL;
static AppWidget      s_widget;
static char           s_http_port[8] = UPG_HTTP_PORT_DEFAULT;
static int            s_http_up = 0;   /* lazy: bound only while an upgrade round runs */

/* Resolve the firmware .bin the aircraft placed in the web root.
 *
 * IMPORTANT: the delivered firmware is a NEW version, whereas this running binary was built with
 * its OWN (older) version — hard-coding SAMPLE_DEVICE_FIRMWARE_VERSION into the filename would
 * look for the old name and never find the new firmware (real-machine must-fail bug). Glob
 * "<name>_V*.bin" first, then ANY "*.bin" (DA2/the aircraft chooses the PUT name), newest mtime
 * wins. Returns 0 and fills out_path (and optionally out_name) on success. */
static int resolve_firmware(char *out_path, size_t path_sz, char *out_name, size_t name_sz)
{
    char pattern[600];
    glob_t g;
    memset(&g, 0, sizeof(g));

    snprintf(pattern, sizeof(pattern), "%s/%s_V*.bin", UPG_WEB_ROOT, SAMPLE_DEVICE_NAME);
    if (glob(pattern, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
        globfree(&g);
        memset(&g, 0, sizeof(g));
        snprintf(pattern, sizeof(pattern), "%s/*.bin", UPG_WEB_ROOT);
        if (glob(pattern, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
            globfree(&g);
            return -1;
        }
    }
    const char *best = NULL;
    time_t best_mtime = 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        struct stat st;
        if (stat(g.gl_pathv[i], &st) != 0 || st.st_size == 0) continue;
        if (!best || st.st_mtime >= best_mtime) { best = g.gl_pathv[i]; best_mtime = st.st_mtime; }
    }
    if (!best) { globfree(&g); return -1; }

    snprintf(out_path, path_sz, "%s", best);
    if (out_name && name_sz) {
        const char *slash = strrchr(best, '/');
        snprintf(out_name, name_sz, "%s", slash ? slash + 1 : best);
    }
    globfree(&g);
    return 0;
}

int upgrade_platform_storage_ok(long *size_out)
{
    char path[700];
    if (resolve_firmware(path, sizeof(path), NULL, 0) != 0) {
        if (size_out) *size_out = 0;
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size == 0) {
        if (size_out) *size_out = 0;
        return 0;
    }
    if (size_out) *size_out = (long)st.st_size;
    return 1;
}

/* Lazily start the firmware HTTP server and build the URL. Lazy because the resident service
 * must not hold the port while idle (would collide with the widget file server / other payload
 * processes). Idempotent: later calls only refresh the URL's filename (the aircraft may have PUT
 * the firmware since). Port-in-use is non-fatal: keep the URL well-formed and let the round fail
 * visibly at download rather than killing the service. */
int upgrade_platform_http_start(const char *fw_name, const char *iface,
                                char *url_out, size_t url_cap)
{
    /* (re)resolve the actual firmware filename each call; fall back to the caller's default. */
    char fw_path[700], name[256];
    if (resolve_firmware(fw_path, sizeof(fw_path), name, sizeof(name)) != 0)
        snprintf(name, sizeof(name), "%s", fw_name ? fw_name : "upgrade.bin");

    char ip[16];
    if (upgrade_http_local_ip(ip, sizeof(ip), iface) != 0)
        LOG_WARN("no suitable interface found (iface=%s), falling back to %s; "
                 "set THING_MODEL_HTTP_IFACE if the aircraft cannot download",
                 iface ? iface : "auto/usb*", ip);

    if (!s_http_up) {
        const char *p = thing_model_sample_getenv("THING_MODEL_UPGRADE_HTTP_PORT", NULL);
        if (p && p[0]) snprintf(s_http_port, sizeof(s_http_port), "%s", p);
        mkdir(UPG_WEB_ROOT, 0755);
        char listen[32];
        snprintf(listen, sizeof(listen), "http://0.0.0.0:%s", s_http_port);
        if (upgrade_http_server_start(listen, UPG_WEB_ROOT) != 0) {
            LOG_ERROR("firmware HTTP server could not bind port %s (in use?); upgrade download "
                      "will fail — set THING_MODEL_UPGRADE_HTTP_PORT to a free port", s_http_port);
            snprintf(url_out, url_cap, "http://%s:%s/%s", ip, s_http_port, name);
            return -1;
        }
        s_http_up = 1;
        LOG_INFO("firmware HTTP server on %s (port %s), web root %s", ip, s_http_port, UPG_WEB_ROOT);
    }
    snprintf(url_out, url_cap, "http://%s:%s/%s", ip, s_http_port, name);
    return 0;
}

void upgrade_platform_http_stop(void)
{
    upgrade_http_server_stop();
    s_http_up = 0;
}

/* Progress + REAL install: overwrite the running binary with the delivered firmware.
 * Returning non-zero aborts the round (the shared state machine logs and stops pushing). */
int upgrade_platform_flash(upgrade_progress_cb_t cb)
{
    for (int p = 0; p <= 100; p += 10) {
        if (cb && cb(p)) return -1;
        ThingModelOsal_SleepMs(500);
    }

    char fw_path[700];
    if (resolve_firmware(fw_path, sizeof(fw_path), NULL, 0) != 0) {
        LOG_ERROR("no firmware found in %s, aborting install", UPG_WEB_ROOT);
        return -1;
    }

    E_UpgApplyMode mode = upg_apply_mode();
    if (mode == UPG_APPLY_STAGE) {
        LOG_INFO("[UPG] firmware staged at %s — STAGE-ONLY: not overwriting the running binary "
                 "(enable real install via samples/upgrade/deploy/: -DUPGRADE_REAL_INSTALL=ON + "
                 "PSDK_UPGRADE_APPLY=1)", fw_path);
        return 0;   /* protocol completes successfully; the host is left untouched */
    }
#ifdef UPGRADE_REAL_INSTALL
    LOG_INFO("installing new firmware %s -> %s%s", fw_path, UpgradePlatformLinux_InstallPath(),
             mode == UPG_APPLY_DRY ? " (dry: install only, no reboot)" : "");
    if (UpgradePlatformLinux_ReplaceOldProgram(fw_path) != 0) {
        LOG_ERROR("firmware install FAILED");
        return -1;
    }
#endif
    return 0;
}

/* Persist "rebooting from a successful upgrade" so the next boot reports COMPLETE. Stage/dry modes
 * persist nothing (there is no reboot to resume from). */
int upgrade_platform_set_boot_pending(void)
{
#ifdef UPGRADE_REAL_INSTALL
    if (upg_apply_mode() == UPG_APPLY_REAL) {
        UpgradePlatformLinux_EnsureAutorunMarker();
        T_UpgradeRebootState rs;
        memset(&rs, 0, sizeof(rs));
        rs.end_state = UPGRADE_END_STATE_SUCCESS;
        snprintf(rs.firmware_version, sizeof(rs.firmware_version), "%s", SAMPLE_DEVICE_FIRMWARE_VERSION);
        if (UpgradePlatformLinux_SetRebootState(&rs) != 0)
            LOG_WARN("could not persist reboot state; COMPLETE may not be reported after the reboot");
    }
#endif
    return 0;
}

/* Reboot only in REAL apply mode. Stage/dry return immediately, so the shared state machine falls
 * through to its COMPLETE loop and the aircraft still gets a well-formed answer. */
void upgrade_platform_reboot(void)
{
#ifdef UPGRADE_REAL_INSTALL
    if (upg_apply_mode() == UPG_APPLY_REAL) {
        ThingModelOsal_SleepMs(1500);   /* let the REBOOTING publish reach the broker first */
        if (UpgradePlatformLinux_RebootSystem() != 0) {
            LOG_ERROR("reboot failed (need root?); continuing without reboot");
            UpgradePlatformLinux_CleanRebootState();
        }
        return;
    }
#endif
    LOG_INFO("[UPG] stage/dry mode: not rebooting (a real deploy would reboot the device here)");
}

/* Post-reboot resume: a persisted reboot state means a previous REAL run installed a new binary and
 * rebooted into us — report COMPLETE. Never true in stage/dry (default) builds. */
int upgrade_platform_has_pending_completion(void)
{
#ifdef UPGRADE_REAL_INSTALL
    int is_reboot = 0;
    T_UpgradeRebootState rs;
    memset(&rs, 0, sizeof(rs));
    (void)UpgradePlatformLinux_GetRebootState(&is_reboot, &rs);
    return is_reboot;
#else
    return 0;
#endif
}

/* Nothing to clear at boot on Linux (the state file doubles as the completion record and is
 * removed by mark_completion_finished once the aircraft acks with finish_upgrade). */
int upgrade_platform_consume_boot_pending(void) { return 1; }

int upgrade_platform_mark_completion_finished(void)
{
#ifdef UPGRADE_REAL_INSTALL
    return UpgradePlatformLinux_CleanRebootState();
#else
    return 0;
#endif
}

/* 0 = stay resident after each round (V3-style always-on service): the standby loop resets and
 * waits for the next upgrade instead of exiting. Ctrl-C still exits at any time. */
int upgrade_platform_round_complete(void) { return 0; }

void upgrade_platform_on_session_ready(ThingModelClientSession_t *session, const char *sn)
{
    /* Background widget get_file_list so DA2 lists the payload as a manageable device. Disable
     * with THING_MODEL_UPGRADE_WIDGET=0 when the standalone widget sample runs on this device
     * (two WidgetImpl instances would double-respond on the shared services topic). */
    if (!atoi(thing_model_sample_getenv("THING_MODEL_UPGRADE_WIDGET", "1"))) return;

    /* assets live next to the binary — resolve independent of the boot cwd. */
    char cfg_dir[600], cfg_json[600];
    app_widget_init(&s_widget, UpgradePlatformLinux_SelfDirPath("config", cfg_dir, sizeof(cfg_dir)));
    s_widget.quiet = 1;
    app_widget_load_config(&s_widget,
        UpgradePlatformLinux_SelfDirPath("config/en/widget_config.json", cfg_json, sizeof(cfg_json)));
    s_wimpl = WidgetImpl_Create(session, sn);
    WidgetImpl_OnGetFileList(s_wimpl, app_widget_on_get_file_list, &s_widget);
    LOG_INFO("registered widget get_file_list (so DA2 lists the device; "
             "set THING_MODEL_UPGRADE_WIDGET=0 if running the widget sample too)");
}

void upgrade_platform_on_session_close(void)
{
    if (s_wimpl) {
        WidgetImpl_Destroy(s_wimpl);
        s_wimpl = NULL;
    }
}
