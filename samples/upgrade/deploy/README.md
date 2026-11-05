# upgrade — real-install deployment (opt-in)

The upgrade **sample** demonstrates the SDK capability only: it answers the aircraft's 4 upgrade
commands, serves the firmware over HTTP, and pushes upgrade state. By default it is **stage-only** —
it writes the received firmware to `/tmp/upgrade_share` and **never overwrites the running binary and
never reboots the host.**

Really installing new firmware and rebooting into it is a *deployment* concern, not something a
shipped sample should do to your machine. It lives here, behind two deliberate gates, so the default
artifact stays inert and only a real rig you control performs the reboot.

## Two gates (both required for a real install)

1. **Compile gate** — `-DUPGRADE_REAL_INSTALL=ON`
   Without it, the install/reboot/boot-state/network code is **not compiled into the binary at all**
   (verify: `strings sample_things_model | grep -c 'rebooting system now'` → `0` in a default build).

2. **Runtime gate** — `PSDK_UPGRADE_APPLY`
   Even in a real-install build the process stays stage-only until armed:
   | value        | behavior                                                        |
   |--------------|-----------------------------------------------------------------|
   | *(unset)*    | stage-only (write firmware, no overwrite, no reboot)            |
   | `dry`        | overwrite the target binary, then **simulate** the reboot (no actual reboot) — full pipeline test on any machine |
   | `1` / `real` | overwrite + persist reboot-state + **reboot** (needs root)      |

## Self-test the real reboot loop on a rig

```bash
# gate 1: compile the real install
cmake -DSAMPLE_BUILD=host -DUPGRADE_REAL_INSTALL=ON -B build -S projects/linux
cmake --build build -j4

# validate the whole pipeline WITHOUT rebooting (any machine):
PSDK_UPGRADE_APPLY=dry ./build/sample_things_model upgrade

# the real thing on the target device (gate 2 = real, needs root):
sudo ./samples/upgrade/deploy/install.sh          # installs the systemd unit with PSDK_UPGRADE_APPLY=1
# aircraft triggers upgrade → firmware staged → binary replaced → systemd restarts → COMPLETE reported
```

## What we do NOT reimplement

Autostart, log capture/rotation, and network bring-up are the init system's job:
- **autostart / restart** → the systemd unit (`WantedBy=multi-user.target`, `Restart=on-failure`)
- **logs** → journald (`journalctl -u psdk-iot-upgrade`)
- **network** → netplan / NetworkManager, or the unit's `ExecStartPre=` hook

The `UpgradePlatformLinux_EnsureNetwork` helper (usb0/bridge/route bring-up for a specific CUBE/RPi
rig) is compiled only under the real-install gate and is best replaced by proper OS network config.
