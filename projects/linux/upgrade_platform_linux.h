/*
 * upgrade_platform_linux.h — real firmware install / reboot / boot-state persistence for the
 *                            Linux (Raspberry Pi) upgrade flow.
 *
 * Modeled on PSDK V3 upgrade_platform_opt_linux.c, adapted to the V4 IoT (Things-Model MQTT) SDK.
 *
 * On Linux the DA2 upgrade file IS the new sample_things_model binary itself (copy
 * sample_things_model to <device_name>_V<ver>.bin for DA2). So "install firmware" means:
 *   overwrite the running binary in place with the newly received .bin, chmod +x, then reboot.
 * The device auto-starts the (now new) binary via systemd (see deploy/ scripts); on the next boot
 * the process detects the persisted reboot-state file and reports UPGRADE COMPLETE to the aircraft.
 *
 * The whole file is POSIX-specific; it is NOT compiled on the RTOS side.
 */
#ifndef UPGRADE_PLATFORM_LINUX_H
#define UPGRADE_PLATFORM_LINUX_H

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- deployment paths ----
 * The install path (the binary the upgrade overwrites) is resolved at RUNTIME in this order:
 *   1. env PSDK_EXCE_FILE   — the product's existing autostart contract (kept as an override).
 *   2. env UPGRADE_INSTALL_BIN_PATH — explicit override.
 *   3. /proc/self/exe       — the running binary itself. This is the ZERO-CONFIG default: the
 *                             marker-driven boot launches the bare binary with no argv/env, and
 *                             the upgrade overwrites exactly the file it is running from.
 *   4. the compile-time UPGRADE_INSTALL_BIN_PATH default below (readlink failure fallback). */
#ifndef UPGRADE_INSTALL_BIN_PATH
#define UPGRADE_INSTALL_BIN_PATH   "/usr/local/bin/sample_things_model"
#endif

#ifndef UPGRADE_REBOOT_STATE_PATH
/* persisted across the reboot so the next boot can report the upgrade result.
 * Absolute path on a persistent (non-tmpfs) fs, independent of the process working dir. */
#define UPGRADE_REBOOT_STATE_PATH  "/var/lib/psdk_iot/upgrade_reboot_state"
#endif

#ifndef UPGRADE_AUTORUN_MARKER_PATH
/* Deployment marker: if this file exists, the launcher (no argv) automatically brings up the
 * resident upgrade task instead of the interactive menu — so the boot entry needs NO parameters.
 * Drop it once at deployment:  mkdir -p /var/lib/psdk_iot && touch /var/lib/psdk_iot/upgrade_autorun
 * (The persisted reboot-state file above independently triggers the same autorun, so the
 * post-upgrade COMPLETE report works even without this marker.) */
#define UPGRADE_AUTORUN_MARKER_PATH "/var/lib/psdk_iot/upgrade_autorun"
#endif

/* upgrade end state persisted across the reboot. */
typedef enum {
    UPGRADE_END_STATE_UNKNOWN = 0,
    UPGRADE_END_STATE_SUCCESS = 1,
    UPGRADE_END_STATE_FAILURE = 2,
} E_UpgradeEndState;

/* persisted reboot-state record (written before reboot, read back after boot). */
typedef struct {
    uint32_t magic;               /* UPGRADE_REBOOT_MAGIC, guards against garbage/partial writes */
    int32_t  end_state;           /* E_UpgradeEndState */
    char     firmware_version[32];/* the version we upgraded to (for logging / cross-check) */
} T_UpgradeRebootState;

/*
 * Compute the MD5 (hex, lowercase, 32 chars + NUL) of a file. Returns 0 on success.
 * Used by check_firmware to verify integrity when the aircraft provides an expected md5.
 */
int UpgradePlatformLinux_FileMd5(const char *path, char out_hex[33]);

/*
 * Return the resolved install path (env PSDK_EXCE_FILE > env UPGRADE_INSTALL_BIN_PATH >
 * compile-time default). The returned pointer is valid until the next call (internal static buf).
 */
const char *UpgradePlatformLinux_InstallPath(void);

/*
 * Replace the running binary at UPGRADE_INSTALL_BIN_PATH with new_firmware_path (cp -f + chmod +x).
 * Returns 0 on success.
 */
int UpgradePlatformLinux_ReplaceOldProgram(const char *new_firmware_path);

/* Persist the upgrade end state so the next boot can report it. Returns 0 on success. */
int UpgradePlatformLinux_SetRebootState(const T_UpgradeRebootState *state);

/*
 * Read the persisted reboot state. On return *is_upgrade_reboot is non-zero iff a valid state
 * file existed (i.e. we just rebooted from an upgrade). Always returns 0 (absence is not an error).
 */
int UpgradePlatformLinux_GetRebootState(int *is_upgrade_reboot, T_UpgradeRebootState *state);

/* Remove the persisted reboot-state file (called after the COMPLETE result has been reported). */
int UpgradePlatformLinux_CleanRebootState(void);

/* Reboot the system (needs root / sudo). Does not return on success. Returns non-zero on failure. */
int UpgradePlatformLinux_RebootSystem(void);

/*
 * Ensure the autorun marker (UPGRADE_AUTORUN_MARKER_PATH) exists — the program maintains the
 * marker itself, no manual `touch` at deployment:
 *   · called when the upgrade sample is EXPLICITLY run (deploy-time verification run), and
 *   · after a successful firmware install (an upgraded device is by definition a resident
 *     deployment, so the service must survive every subsequent boot).
 * Running other samples never creates it (a debug run of camera/power must not change the
 * machine's boot behavior). Needs root (/var/lib); failure is logged and non-fatal.
 * Delete the file to restore the interactive menu on bare launches.
 */
void UpgradePlatformLinux_EnsureAutorunMarker(void);

/*
 * Should the launcher auto-start the resident upgrade task on a bare (no-argv) launch?
 * Non-zero iff the autorun marker or a persisted upgrade reboot state exists.
 * (Launcher-facing probe; pure stat(), touches no upgrade state.)
 */
int upgrade_autorun_pending(void);

/*
 * Resolve `rel` against the RUNNING BINARY's directory (/proc/self/exe) into buf.
 * Falls back to returning `rel` unchanged (cwd-relative) if self-exe is unavailable.
 * Bundled assets (./config) must be found next to the binary regardless of the boot cwd.
 */
const char *UpgradePlatformLinux_SelfDirPath(const char *rel, char *buf, size_t cap);

/*
 * Ensure the aircraft-facing network is up so the broker is reachable (test-rig helper,
 * integrated into the upgrade autostart flow — critical right after an upgrade reboot, where
 * the fresh boot must FIRST configure the network and only then connect to report COMPLETE).
 *
 * Behavior (all steps idempotent, non-fatal, root required for the config commands):
 *   0. Enabled when the autorun marker or a persisted reboot state exists (i.e. the resident
 *      upgrade deployment), or forced with env THING_MODEL_NET_SETUP=1; disabled with =0.
 *   1. Probe broker_host (1s ping). Reachable -> nothing to do.
 *   2. DHCP the uplink iface        (env THING_MODEL_NET_UPLINK, default usb0).
 *   3. If a bridge iface exists     (env THING_MODEL_NET_BRIDGE, default pi4br0),
 *      move the uplink's IP onto it (the CUBE/RPi rig topology); otherwise stay on the uplink.
 *   4. Route the broker subnet      (env THING_MODEL_NET_BROKER_SUBNET, default 192.0.2.0/24)
 *      via the uplink's DHCP gateway.
 * Returns 0 if the broker is reachable afterwards, non-zero otherwise (caller retries anyway).
 */
int UpgradePlatformLinux_EnsureNetwork(const char *broker_host);

#ifdef __cplusplus
}
#endif

#endif /* __linux__ */
#endif /* UPGRADE_PLATFORM_LINUX_H */
