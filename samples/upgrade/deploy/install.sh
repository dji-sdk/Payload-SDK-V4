#!/usr/bin/env bash
# install.sh — arm the opt-in real-upgrade deployment on a target device.
#
# This is the deliberate "arming" step: it installs the binary + a systemd unit that runs the
# resident upgrade service with the real-install runtime gate on. Run it only on a device you own
# and intend to be auto-upgraded (it will reboot itself when the aircraft pushes new firmware).
#
# Prerequisite: build with the compile gate on:
#   cmake -DSAMPLE_BUILD=host -DUPGRADE_REAL_INSTALL=ON -B build -S projects/linux && cmake --build build -j4
#
# Usage:  sudo ./install.sh [path/to/sample_things_model]
set -euo pipefail

BIN_SRC="${1:-build/sample_things_model}"
BIN_DST="/usr/local/bin/sample_things_model"
UNIT_SRC="$(dirname "$0")/psdk-iot-upgrade.service"
UNIT_DST="/etc/systemd/system/psdk-iot-upgrade.service"
NET_SRC="$(dirname "$0")/psdk-net-setup.sh"
NET_DST="/usr/local/bin/psdk-net-setup.sh"

if [[ $EUID -ne 0 ]]; then echo "must run as root (sudo $0)"; exit 1; fi
# Only require the source to EXIST and be readable — a freshly scp'd binary often loses its +x bit,
# and we install it with `install -Dm755` (which sets 0755 on the destination) anyway. The
# real-install grep below reads the file, it does not execute it.
if [[ ! -f "$BIN_SRC" ]]; then echo "binary not found: $BIN_SRC"; exit 1; fi

# Refuse to arm a stage-only build — the reboot code must actually be compiled in. Grep the binary
# file DIRECTLY (do not use `strings | grep -q`: under `set -o pipefail`, grep -q closes the pipe on
# the first match, strings then dies with SIGPIPE=141, and that non-zero pipeline status flips the
# `if !` test so a valid real-install binary gets wrongly rejected). grep -a on the file has no pipe
# to break and needs no `strings` on the target.
if ! grep -aq 'rebooting system now' "$BIN_SRC"; then
    echo "ERROR: '$BIN_SRC' was built WITHOUT -DUPGRADE_REAL_INSTALL=ON (stage-only); it cannot really install."
    exit 1
fi

install -Dm755 "$BIN_SRC"  "$BIN_DST"
# ship the widget config bundle next to the binary (upgrade registers widget get_file_list; the
# runtime resolves ./config against /proc/self/exe, i.e. $(dirname "$BIN_DST")/config). The bundle
# travels as a plain directory, NOT inside the firmware — OTA won't carry it, so a release that
# changes widget resources needs a manual re-push of config/ (see deploy/TEST_GUIDE.md).
if [[ -d "$(dirname "$BIN_SRC")/config" ]]; then
    rm -rf "$(dirname "$BIN_DST")/config"
    cp -r "$(dirname "$BIN_SRC")/config" "$(dirname "$BIN_DST")/"
    echo "installed widget config -> $(dirname "$BIN_DST")/config"
else
    echo "WARN: no config/ next to '$BIN_SRC' — DA2 widget list will be empty until you push it:"
    echo "      scp -r build/config <dev>:/tmp/config && sudo cp -r /tmp/config $(dirname "$BIN_DST")/config"
fi
install -Dm644 "$UNIT_SRC" "$UNIT_DST"
# ship the aircraft-facing network bring-up hook referenced by the unit's ExecStartPre
install -Dm755 "$NET_SRC" "$NET_DST"

# ship the bundled shared libs if present. The binary's RPATH is $ORIGIN/lib, so they must land in
# $(dirname "$BIN_DST")/lib. Needed when the target lacks the exact NEEDED versions system-wide
# (libmosquitto.so.1, libssl.so.1.1, libcrypto.so.1.1) — RPATH is searched first, so bundled wins.
if [[ -d "$(dirname "$BIN_SRC")/lib" ]]; then
    install -d "$(dirname "$BIN_DST")/lib"
    cp -a "$(dirname "$BIN_SRC")/lib/." "$(dirname "$BIN_DST")/lib/"
    echo "installed bundled libs -> $(dirname "$BIN_DST")/lib"
else
    echo "note: no lib/ next to '$BIN_SRC' — relying on system libmosquitto/libssl/libcrypto;"
    echo "      if the service fails with 'cannot open shared object', push lib/ to $(dirname "$BIN_DST")/lib"
fi

systemctl daemon-reload
systemctl enable --now psdk-iot-upgrade.service

echo "armed: psdk-iot-upgrade.service running (PSDK_UPGRADE_APPLY=1)."
echo "  logs:   journalctl -u psdk-iot-upgrade -f"
echo "  disarm: sudo systemctl disable --now psdk-iot-upgrade.service"
