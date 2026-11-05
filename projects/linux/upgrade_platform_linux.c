/*
 * upgrade_platform_linux.c — real firmware install / reboot / boot-state persistence (Raspberry Pi).
 *
 * See upgrade_platform_linux.h for the deployment model. Modeled on PSDK V3
 * upgrade_platform_opt_linux.c but adapted to the V4 IoT SDK and self-contained (no dji_* deps).
 */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include "upgrade_platform_linux.h"
#include "widget_md5.h"      /* reuse the vendored MD5 (also compiled into the upgrade sample) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <unistd.h>
#if defined(__linux__)
#  include <sys/reboot.h>   /* reboot(2), RB_AUTOBOOT */
#  include <linux/reboot.h>
#endif

#define UPGRADE_REBOOT_MAGIC   0x55504752u  /* 'UPGR' */

/* ── Dangerous host-mutating helpers (binary overwrite / reboot / network config) are compiled
 *    ONLY for the opt-in real-install deploy build. The default sample artifact contains none of
 *    them — see samples/upgrade/deploy/ and the UPGRADE_REAL_INSTALL build option. ── */
#ifdef UPGRADE_REAL_INSTALL

/* Run a shell command, return 0 iff it exited 0. */
static int run_cmd(const char *cmd)
{
    printf("    [platform] %s\n", cmd);
    int status = system(cmd);
    if (status == -1) {
        printf("    [platform] system() failed: %s\n", strerror(errno));
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("    [platform] command exited %d\n", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

/* mkdir -p the directory that `path`'s file lives in. */
static void ensure_parent_dir(const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *dir = dirname(tmp);      /* modifies tmp in place */
    if (dir && dir[0]) {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
        (void)run_cmd(cmd);
    }
}

#endif /* UPGRADE_REAL_INSTALL */

int UpgradePlatformLinux_FileMd5(const char *path, char out_hex[33])
{
    if (!path || !out_hex) return -1;
    return widget_md5_file(path, out_hex);   /* 0 on success, -1 on open/read failure */
}

/* True iff p names an existing regular file. */
static int is_regular_file(const char *p)
{
    struct stat st;
    return p && p[0] && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Resolve the running binary's own path via /proc/self/exe into buf; NULL on failure. */
static const char *self_exe_path(char *buf, size_t cap)
{
    ssize_t n = readlink("/proc/self/exe", buf, cap - 1);
    if (n <= 0) return NULL;
    buf[n] = '\0';
    /* " (deleted)" suffix appears if the running inode was already replaced (e.g. a
     * previous upgrade round in the same process life) — strip it, the path is still right. */
    char *del = strstr(buf, " (deleted)");
    if (del) *del = '\0';
    return buf;
}

const char *UpgradePlatformLinux_InstallPath(void)
{
    static char resolved[512];

    /* 1./2. env overrides — the product's autostart contract (PSDK_EXCE_FILE) or an explicit
     *       path. VALIDATED: the target must be an existing regular file. A stale value (e.g.
     *       PSDK_EXCE_FILE=/not/exist/none left in /etc/environment by an autostart recovery)
     *       would otherwise silently install the new firmware into a dead path — the real
     *       binary stays old and the version never changes after the upgrade reboot. */
    const char *p = getenv("PSDK_EXCE_FILE");
    if (!p || !p[0]) p = getenv("UPGRADE_INSTALL_BIN_PATH");
    if (p && p[0]) {
        char cand[512];
        if (p[0] != '/') {
            /* Relative path: reproduce rc.local's semantics (cd ${PSDK_TOP_DIR} first). */
            const char *top = getenv("PSDK_TOP_DIR");
            if (top && top[0]) {
                const char *rel = p;
                if (rel[0] == '.' && rel[1] == '/') rel += 2;
                size_t tlen = strlen(top);
                while (tlen > 1 && top[tlen - 1] == '/') tlen--;
                snprintf(cand, sizeof(cand), "%.*s/%s", (int)tlen, top, rel);
            } else {
                cand[0] = '\0';   /* relative with no anchor — unusable */
            }
        } else {
            snprintf(cand, sizeof(cand), "%s", p);
        }
        if (is_regular_file(cand)) {
            snprintf(resolved, sizeof(resolved), "%s", cand);
            return resolved;
        }
        printf("    [platform] WARN: install path from env ('%s') is not an existing file — "
               "stale deployment residue? Falling back to the running binary (/proc/self/exe).\n", p);
    }

    /* 3. self path — the ZERO-CONFIG default: overwrite exactly the binary we are running from. */
    if (self_exe_path(resolved, sizeof(resolved))) return resolved;

    /* 4. compile-time default (readlink failure fallback). */
    snprintf(resolved, sizeof(resolved), "%s", UPGRADE_INSTALL_BIN_PATH);
    return resolved;
}

#ifdef UPGRADE_REAL_INSTALL
int UpgradePlatformLinux_ReplaceOldProgram(const char *new_firmware_path)
{
    if (!new_firmware_path) return -1;

    struct stat st;
    if (stat(new_firmware_path, &st) != 0 || st.st_size == 0) {
        printf("    [platform] new firmware missing or empty: %s\n", new_firmware_path);
        return -1;
    }

    const char *install_path = UpgradePlatformLinux_InstallPath();
    char cmd[1200];
    /* copy to a temp path on the same filesystem, then atomically rename over the running binary,
     * so a crash mid-copy can never leave a truncated (unbootable) binary in place. */
    char tmp_path[600];
    snprintf(tmp_path, sizeof(tmp_path), "%s.new", install_path);

    ensure_parent_dir(install_path);

    snprintf(cmd, sizeof(cmd), "cp -f '%s' '%s'", new_firmware_path, tmp_path);
    if (run_cmd(cmd) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "chmod 755 '%s'", tmp_path);
    if (run_cmd(cmd) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "mv -f '%s' '%s'", tmp_path, install_path);
    if (run_cmd(cmd) != 0) return -1;

    /* flush to disk so the new binary survives the reboot below. */
    (void)run_cmd("sync");

    printf("    [platform] installed new firmware -> %s\n", install_path);
    return 0;
}

int UpgradePlatformLinux_SetRebootState(const T_UpgradeRebootState *state)
{
    if (!state) return -1;
    ensure_parent_dir(UPGRADE_REBOOT_STATE_PATH);

    FILE *f = fopen(UPGRADE_REBOOT_STATE_PATH, "wb");
    if (!f) {
        printf("    [platform] cannot write reboot state %s: %s\n",
               UPGRADE_REBOOT_STATE_PATH, strerror(errno));
        return -1;
    }
    T_UpgradeRebootState rec = *state;
    rec.magic = UPGRADE_REBOOT_MAGIC;
    size_t n = fwrite(&rec, 1, sizeof(rec), f);
    fclose(f);
    if (n != sizeof(rec)) {
        printf("    [platform] short write on reboot state\n");
        return -1;
    }
    (void)run_cmd("sync");
    printf("    [platform] persisted reboot state (end_state=%d ver=%s)\n",
           rec.end_state, rec.firmware_version);
    return 0;
}

int UpgradePlatformLinux_GetRebootState(int *is_upgrade_reboot, T_UpgradeRebootState *state)
{
    if (is_upgrade_reboot) *is_upgrade_reboot = 0;

    FILE *f = fopen(UPGRADE_REBOOT_STATE_PATH, "rb");
    if (!f) return 0;   /* absence is normal, not an error */

    T_UpgradeRebootState rec;
    size_t n = fread(&rec, 1, sizeof(rec), f);
    fclose(f);
    if (n != sizeof(rec) || rec.magic != UPGRADE_REBOOT_MAGIC) {
        printf("    [platform] reboot state present but invalid, ignoring\n");
        return 0;
    }
    if (state) *state = rec;
    if (is_upgrade_reboot) *is_upgrade_reboot = 1;
    printf("    [platform] detected upgrade reboot (end_state=%d ver=%s)\n",
           rec.end_state, rec.firmware_version);
    return 0;
}

int UpgradePlatformLinux_CleanRebootState(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -f '%s'", UPGRADE_REBOOT_STATE_PATH);
    return run_cmd(cmd);
}

int UpgradePlatformLinux_RebootSystem(void)
{
    printf("    [platform] rebooting system now...\n");

    /* Flush filesystem buffers to disk BEFORE issuing the reboot. sync(3) is the libc call
     * (equivalent to the `sync` command) and always returns. */
    sync();

#if defined(__linux__)
    /* Preferred path: the reboot(2) syscall. Unlike system("reboot") it does NOT depend on an
     * external `reboot` binary or on a specific init/systemd being present — it asks the kernel
     * to reboot directly and does not block waiting for a userspace init to acknowledge.
     *
     * On some customized systems (Android-like, busybox, non-systemd init) the `reboot` COMMAND
     * blocks forever talking to an init that never reboots, which is exactly the "stuck after
     * [platform] reboot" symptom. The syscall avoids that.
     *
     * Note: RB_AUTOBOOT performs an immediate hard reboot (no clean service shutdown). We already
     * sync()'d and persisted the reboot-state file, so this is safe for our purpose. Requires
     * CAP_SYS_BOOT (root). */
    reboot(RB_AUTOBOOT);
    /* reboot(2) returned → it failed (e.g. EPERM: not privileged). Fall through to command fallbacks. */
    printf("    [platform] reboot(2) syscall returned (errno=%d %s), trying commands...\n",
           errno, strerror(errno));
#endif

    /* Fallbacks for non-Linux or when the syscall was denied. Each is best-effort. */
    if (run_cmd("reboot -f") == 0) return 0;       /* -f = force, skips talking to init */
    if (run_cmd("busybox reboot -f") == 0) return 0;
    if (run_cmd("reboot") == 0) return 0;
    if (run_cmd("systemctl reboot") == 0) return 0;

    printf("    [platform] reboot failed (need root / CAP_SYS_BOOT?)\n");
    return -1;
}

int upgrade_autorun_pending(void)
{
    struct stat st;
    if (stat(UPGRADE_AUTORUN_MARKER_PATH, &st) == 0) return 1;
    if (stat(UPGRADE_REBOOT_STATE_PATH,  &st) == 0) return 1;
    return 0;
}
#endif /* UPGRADE_REAL_INSTALL */

const char *UpgradePlatformLinux_SelfDirPath(const char *rel, char *buf, size_t cap)
{
    char exe[512];
    if (!self_exe_path(exe, sizeof(exe))) return rel;
    char *slash = strrchr(exe, '/');
    if (!slash) return rel;
    *slash = '\0';
    snprintf(buf, cap, "%s/%s", exe, rel);
    return buf;
}

#ifdef UPGRADE_REAL_INSTALL
void UpgradePlatformLinux_EnsureAutorunMarker(void)
{
    struct stat st;
    if (stat(UPGRADE_AUTORUN_MARKER_PATH, &st) == 0) return;   /* already there */

    ensure_parent_dir(UPGRADE_AUTORUN_MARKER_PATH);
    FILE *f = fopen(UPGRADE_AUTORUN_MARKER_PATH, "w");
    if (!f) {
        printf("    [platform] could not create autorun marker %s (%s) — bare boots will show "
               "the menu instead of the resident upgrade task (run as root to persist)\n",
               UPGRADE_AUTORUN_MARKER_PATH, strerror(errno));
        return;
    }
    fclose(f);
    printf("    [platform] autorun marker created: %s (bare launches now auto-start the resident "
           "upgrade task; delete the file to restore the interactive menu)\n",
           UPGRADE_AUTORUN_MARKER_PATH);
}

/* quiet ping probe; 0 = host reachable. */static int host_reachable(const char *host)
{
    if (!host || !host[0]) return -1;
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 '%s' >/dev/null 2>&1", host);
    int status = system(cmd);
    return (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

int UpgradePlatformLinux_EnsureNetwork(const char *broker_host)
{
    /* Gate: explicit env wins; default = only on resident-upgrade deployments (autorun marker or
     * a persisted reboot state present). Dev/interactive runs stay untouched. */
    const char *en = getenv("THING_MODEL_NET_SETUP");
    if (en && en[0]) {
        if (!atoi(en)) return 0;                      /* THING_MODEL_NET_SETUP=0: disabled */
    } else {
        struct stat st;
        if (stat(UPGRADE_AUTORUN_MARKER_PATH, &st) != 0 &&
            stat(UPGRADE_REBOOT_STATE_PATH,  &st) != 0)
            return 0;                                  /* not a resident deployment */
    }

    /* Already reachable (previous boot config persisted, or another path) -> nothing to do. */
    if (host_reachable(broker_host) == 0) return 0;

    const char *up = getenv("THING_MODEL_NET_UPLINK");        if (!up || !up[0]) up = "usb0";
    const char *br = getenv("THING_MODEL_NET_BRIDGE");        if (!br || !br[0]) br = "pi4br0";
    const char *sn = getenv("THING_MODEL_NET_BROKER_SUBNET"); if (!sn || !sn[0]) sn = "192.0.2.0/24";

    printf("    [platform] broker %s unreachable — configuring network (uplink=%s bridge=%s subnet=%s)\n",
           broker_host ? broker_host : "?", up, br, sn);

    /* 1. DHCP the aircraft-facing uplink (idempotent; -1 = single attempt, don't daemonize forever). */
    char cmd[900];
    snprintf(cmd, sizeof(cmd), "dhclient -1 '%s' >/dev/null 2>&1", up);
    (void)run_cmd(cmd);

    /* 2+3. Read the uplink's DHCP lease; if the rig bridge exists move the IP onto it (CUBE/RPi
     *      topology), else keep it on the uplink; then route the broker subnet via the DHCP gw.
     *      One shell so CIDR/GW variables can be reused; every step guarded and idempotent. */
    snprintf(cmd, sizeof(cmd),
        "CIDR=$(ip -4 -o addr show dev '%s' | awk '{print $4}' | head -1); "
        "GW=$(ip -4 -o route show default dev '%s' | awk '/via/{for(i=1;i<=NF;i++) if($i==\"via\") print $(i+1)}' | head -1); "
        "[ -z \"$CIDR\" ] && CIDR=$(ip -4 -o addr show dev '%s' | awk '{print $4}' | head -1); "  /* IP may already sit on the bridge */
        "[ -n \"$CIDR\" ] || exit 1; "
        "DEV='%s'; "
        "if ip link show '%s' >/dev/null 2>&1; then "
        "  DEV='%s'; "
        "  ip addr del $CIDR dev '%s' 2>/dev/null; "
        "  ip addr replace $CIDR dev '%s'; "
        "fi; "
        "if [ -n \"$GW\" ]; then ip route replace '%s' via $GW dev $DEV src ${CIDR%%/*}; fi",
        up, up, br,
        up,
        br, br, up, br,
        sn);
    (void)run_cmd(cmd);

    if (host_reachable(broker_host) == 0) {
        printf("    [platform] network up, broker %s reachable\n", broker_host);
        return 0;
    }
    printf("    [platform] broker still unreachable (caller keeps retrying; check cabling/topology "
           "or override THING_MODEL_NET_UPLINK/_BRIDGE/_BROKER_SUBNET)\n");
    return -1;
}
#endif /* UPGRADE_REAL_INSTALL */

#endif /* __linux__ */
