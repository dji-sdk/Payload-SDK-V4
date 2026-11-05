# Firmware Upgrade Test Guide

> 语言 / Language: [中文](TEST_GUIDE.md) | **English**

This guide walks through upgrading the Payload firmware on a Raspberry Pi via DJI Assistant 2 (DA2),
covering the full loop: **receive firmware → overwrite the running binary → reboot → auto-start the
new version on boot → report COMPLETE**.

Deployment shape: **single binary + resident systemd service**. On boot systemd launches
`sample_things_model` with no arguments; the program detects the upgrade marker / reboot-state file
and automatically enters the resident upgrade task (parked in the background):

- Shared libraries resolve from `./lib` next to the binary (rpath `$ORIGIN/lib`), so no
  `LD_LIBRARY_PATH` / `ldconfig` needed;
- The widget config resolves from `./config` next to the binary, independent of the launch CWD;
- The firmware install path defaults to `/proc/self/exe` (overwrite the running binary itself), no
  arguments required.

## 0. Two gates (both required for real install + reboot)

By default the sample is **stage-only**: it drives the full protocol but **never overwrites the
binary and never reboots**. Real-device testing requires opening both gates:

1. **Compile gate** `-DUPGRADE_REAL_INSTALL=ON`
   Without it the install / reboot / reboot-state / network code is not compiled in at all.
   Verify: `strings sample_things_model | grep -c 'rebooting system now'` → `1` for a real build, `0` for stage-only.
2. **Runtime gate** `PSDK_UPGRADE_APPLY` (injected by the systemd unit)
   | value | behavior |
   |---|---|
   | unset | stage-only (write firmware, no overwrite, no reboot) |
   | `dry` | overwrite the target binary but **do not reboot** (validate the whole pipeline on any machine) |
   | `1`/`real` | overwrite + persist reboot-state + **reboot** (needs root) |

> `deploy/install.sh` verifies gate 1 is actually compiled in (via `strings`) and installs a
> systemd unit carrying `PSDK_UPGRADE_APPLY=1` to close gate 2 for you.

## 1. Build the real-install firmware (dev host, aarch64 cross)

```bash
cmake -B build -S projects/linux \
  -DSAMPLE_BUILD=cross \
  -DUPGRADE_REAL_INSTALL=ON \
  -DSAMPLE_DEVICE_FIRMWARE_VERSION=01.00.00.00
cmake --build build -j"$(nproc)"
# verify the real-install code is compiled in (should print 1)
strings build/sample_things_model | grep -c 'rebooting system now'
```

Artifacts: `build/sample_things_model` (self-contained, rpath `$ORIGIN/lib`), `build/config/`.

## 2. Deploy to the device and "arm" autostart

Copy the binary + config + deploy scripts to the device (example dir `~/psdk/`):

```bash
scp -r build/sample_things_model build/config samples/upgrade/deploy  rsp@<device-ip>:~/psdk/
# If the system has no pre-installed deps, also copy lib/ (libmosquitto/libssl/libcrypto) next to the binary:
# scp -r samples/third_party/aarch64-linux-gnu/lib  rsp@<device-ip>:~/psdk/lib
```

Log in to the device and use the bundled systemd installer (needs root):

```bash
ssh rsp@<device-ip>
cd ~/psdk
sudo ./deploy/install.sh ./sample_things_model
```

`install.sh` does four things:
1. installs the binary to `/usr/local/bin/sample_things_model` (+ `config/` alongside, + `lib/` if present);
2. installs the network pre-hook to `/usr/local/bin/psdk-net-setup.sh`;
3. installs the systemd unit `psdk-iot-upgrade.service` (`ExecStart=/usr/local/bin/sample_things_model`,
   `WorkingDirectory=/usr/local/bin`, `Environment=PSDK_UPGRADE_APPLY=1`,
   `ExecStartPre=-/usr/local/bin/psdk-net-setup.sh`);
4. `systemctl enable --now` — the service goes resident immediately and auto-starts on every boot.

```bash
journalctl -u psdk-iot-upgrade -f     # live log
```

You should see: (network setup) → connect to broker → "Registered 4 upgrade command handlers, waiting...".

> To keep it running after closing the terminal / hand it to autostart: just `sudo reboot`, systemd
> brings it up. The upgrade marker `/var/lib/psdk_iot/upgrade_autorun` and the reboot-state file
> `/var/lib/psdk_iot/upgrade_reboot_state` are self-maintained by the program — no manual `touch`.
> Delete the marker to restore the interactive-menu behavior on a bare launch.

### config widget resource bundle (pushed manually, travels as a directory)

The widget resources (`widget_config.json` + png icons under `config/en`, `config/zh`) let DA2 list
the Payload as a manageable device and render its widget UI. The program resolves them against the
**running binary's own directory** in `on_session_ready` (`SelfDirPath("config")`, CWD-independent),
so the bundle must land at:

```
/usr/local/bin/config/           ← alongside /usr/local/bin/sample_things_model
├── en/{widget_config.json, *.png}
└── zh/{widget_config.json, *.png}
```

- **First deploy**: `install.sh` above already copies `~/psdk/config` to `/usr/local/bin/config` — nothing extra to do.
- **Push / re-push on its own** (e.g. changed icons or `widget_config.json`): push the directory
  manually, **no rebuild needed**:

  ```bash
  scp -r build/config  rsp@<device-ip>:/tmp/config
  ssh rsp@<device-ip> 'sudo rm -rf /usr/local/bin/config && sudo cp -r /tmp/config /usr/local/bin/config'
  ssh rsp@<device-ip> 'sudo systemctl restart psdk-iot-upgrade'   # re-register get_file_list
  ```

> ⚠️ **OTA does not carry config**: a firmware upgrade only atomically replaces the binary itself
> (see section 5); `config/` is left as-is. If a new version **changes widget resources**, you must
> **manually re-push** the `config` directory to `/usr/local/bin/config` after the upgrade (commands
> above); if the resources are unchanged, nothing to do.
> A missing config does not affect the upgrade itself: the 4 upgrade commands + reboot + COMPLETE are
> unrelated to the widget; only DA2's widget list returns "directory not found" (you can disable the
> widget registration explicitly with `THING_MODEL_UPGRADE_WIDGET=0`).

### Network configuration (pre-hook, not built into the binary)

After the upgrade reboot the fresh process must get on the network before it can dial the Broker to
report COMPLETE. Network bring-up is handled by the unit's
`ExecStartPre=-/usr/local/bin/psdk-net-setup.sh` (installed by `install.sh`). The script is
idempotent: wait for the uplink's DHCP lease → probe the Broker → if unreachable, move the IP onto
the bridge (when a bridge exists) → add the Broker-subnet route.

> **Topology note**: on this carrier board `usb0` is a bridge port of `pi4br0` (they share a MAC);
> a bridge port only does L2 forwarding and cannot emit L3 packets. So the script **moves** the
> aircraft-subnet IP from usb0 **onto pi4br0** and adds the route on pi4br0 too
> (`ip route add <broker-subnet> via <gateway> dev pi4br0 src <aircraft-ip>`), matching the repo-root
> `main.c` logic.

The default topology matches this carrier board; override in the unit via `Environment=`:

| Env var | Default | Meaning |
|---|---|---|
| `THING_MODEL_NET_UPLINK` | `usb0` | aircraft-facing DHCP NIC |
| `THING_MODEL_NET_BRIDGE` | `pi4br0` | IP-migration target bridge (stay on the uplink if absent) |
| `THING_MODEL_NET_BROKER_SUBNET` | `192.0.2.0/24` | Broker subnet to route |
| `THING_MODEL_NET_GATEWAY` | auto-detect | next hop to the Broker subnet; **must be set explicitly** when the uplink has no default route, otherwise the route is not added |
| `THING_MODEL_NET_WAIT` | `45` | seconds to wait at boot for the uplink to appear and get a lease (the USB gadget/bridge come up after the service, so a wait is required) |
| `THING_MODEL_BROKER` | broker from the connection config | if already pingable the script skips network setup entirely |

> If the device already brings the aircraft-facing network up via netplan / NetworkManager, just
> delete the `ExecStartPre` line from the unit.
> Clean up stale residue: if `/etc/environment` still has a `PSDK_EXCE_FILE` pointing at a
> non-existent file (e.g. `/not/exist/none` written by the restore step), the program warns and falls
> back to overwriting the running binary itself — but remove that stale line to avoid confusion.

## 3. Confirm the service is online

```bash
ps aux | grep sample_things_model        # process is up
sudo systemctl status psdk-iot-upgrade   # service active (running)
journalctl -u psdk-iot-upgrade -f        # live log: network → connect broker → upgrade standby
```

The Payload should appear in DA2's device list. The autostart log can also be persisted to
`/var/log/psdk_iot.log` (`THING_MODEL_LOG_FILE` changes the path, `0`/`off` disables it; rotates to
`.1` past 8 MB). Each boot banner carries the firmware version and pid:

```bash
grep "autorun boot" /var/log/psdk_iot.log   # per-boot banners; cross-check the post-upgrade version
```

## 4. Upgrade

1. Connect the device in DA2 and open the "Firmware Upgrade" page.
2. Select a **new-version** firmware file (see "5. Verify the version change"), click upgrade.
3. Wait for progress to finish. The log runs the full chain:

   ```
   enter_upgrade_mode → HTTP server on ... (web root /tmp/upgrade_share)
   [http] PUT /rpi_test_V<new-ver>.bin -> saved N bytes     # aircraft PUTs the firmware to the device
   check_firmware -> passed
   [UPG] Flash progress 0..100%
   installing new firmware /tmp/upgrade_share/... -> /usr/local/bin/sample_things_model
   persisted reboot state (end_state=1 ver=<new-ver>)
   pushing REBOOTING → [platform] rebooting system now...
   ```
4. The device reboots automatically → systemd starts the **new** `/usr/local/bin/sample_things_model`
   (detects the reboot-state file and goes resident) → pushes COMPLETE → DA2 shows upgrade success and
   the firmware version updates to the new one.

## 5. Verify the version change: prepare a "new-version" firmware

The firmware version is baked in at compile time via `SAMPLE_DEVICE_FIRMWARE_VERSION` (single source
of truth `sample_device_info.h`). To observe the version change after an upgrade, the **version
running on the device** must differ from the **version pushed via DA2**: install v1 on the device,
push v2 via DA2.

```bash
# v2: a higher version, used as the upgrade package (the same binary at a higher version number)
cmake -B build_v2 -S projects/linux \
  -DSAMPLE_BUILD=cross -DUPGRADE_REAL_INSTALL=ON \
  -DSAMPLE_DEVICE_FIRMWARE_VERSION=01.00.00.01
cmake --build build_v2 -j"$(nproc)"
cp build_v2/sample_things_model  rpi_test_V01.00.00.01.bin   # copy to the DA2 PC, select it when upgrading
```

> ⚠️ A single-binary upgrade only replaces the binary file itself; it does **not** replace `config/`
> or `lib/`. If a new version changes widget resources (`config`) or shared libs (`lib`), you must
> manually re-push the corresponding directory to `/usr/local/bin/` after the upgrade (config push
> command in "2 · config widget resource bundle"); nothing to do if only the binary changed.

## 6. (Optional) dry-run rehearsal before the real reboot

Before rebooting real hardware, you can run the whole "overwrite the binary but do not reboot"
pipeline on any machine with `dry` mode:

```bash
cmake -B build_host -S projects/linux -DSAMPLE_BUILD=host -DUPGRADE_REAL_INSTALL=ON
cmake --build build_host -j"$(nproc)"
PSDK_UPGRADE_APPLY=dry ./build_host/sample_things_model upgrade
# the pipeline is fine once the log shows "(dry: install only, no reboot)"
```

## 7. Restore the environment

```bash
sudo systemctl disable --now psdk-iot-upgrade.service
sudo rm -f /etc/systemd/system/psdk-iot-upgrade.service \
           /usr/local/bin/sample_things_model \
           /usr/local/bin/psdk-net-setup.sh
sudo rm -rf /usr/local/bin/config /usr/local/bin/lib \
            /var/lib/psdk_iot /tmp/upgrade_share      # clear config/lib/markers/state/staged firmware
sudo systemctl daemon-reload
sudo reboot
```

## 8. Troubleshooting

| Symptom | Fix |
|---|---|
| DA2 can't see the device | Confirm the service is active (`systemctl status psdk-iot-upgrade`) and can reach the Broker |
| Program exits immediately (missing .so) | Put `lib/` (libmosquitto/libssl/libcrypto) next to the binary, or install system-wide + `ldconfig` |
| Upgrade "succeeds" but version unchanged | ① binary is a stage build (`strings ... grep -c 'rebooting system now'` is 0 → rebuild with `-DUPGRADE_REAL_INSTALL=ON`); ② you ran it manually from a different dir and overwrote another copy — always use the systemd one at `/usr/local/bin/...`; ③ the firmware version selected in DA2 matches the current one |
| Upgrade progress stalls | Confirm the device is on the network and can reach the Broker; check whether `psdk-net-setup.sh` configured successfully (`[psdk-net-setup]` lines in `journalctl`) |
| Stuck at `[platform] rebooting` | reboot needs root/CAP_SYS_BOOT; fine when systemd runs the unit as root |
| widget `load_config ... FAIL` | the widget sample resolves resources relative to `./config`; the unit needs `WorkingDirectory=/usr/local/bin` (built into this repo's unit), or confirm `/usr/local/bin/config` exists |
| Stuck at CONNACK after a restart | duplicate connections fighting over the same client_id (a stray manual instance / stale session); `pkill -f sample_things_model` then restart, or reboot the whole device |
