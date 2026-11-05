#!/usr/bin/env bash
# ============================================================
# vendor-deps.sh — collect third-party dependencies (headers + precompiled .so) into third_party/
#                  so the SDK build does not depend on the host system's headers/libraries.
#
# Layout (one directory per GNU triplet, each a self-contained mini-sysroot):
#   third_party/<triplet>/include/openssl/*.h   OpenSSL 1.1.1 public headers + per-arch opensslconf.h
#   third_party/<triplet>/include/mosquitto.h   libmosquitto header
#   third_party/<triplet>/lib/lib{ssl,crypto,mosquitto}.so*
#
#   triplet in { x86_64-linux-gnu (host), aarch64-linux-gnu (target board) }
#
# Pinned versions: OpenSSL 1.1.1 / libmosquitto 1.6.9 (matching the bundled aarch64 .so).
#
# Notes:
#   * x86_64 .so/headers are collected from the local system packages (needs libssl-dev libmosquitto-dev).
#   * aarch64 .so are prebuilt for the target and cannot be regenerated on an x86 host; they are
#     bundled. This script only refreshes the aarch64 OpenSSL headers (public headers are
#     arch-independent; only opensslconf.h comes from the arm64 package).
#   * Re-run this script after upgrading OpenSSL/mosquitto or switching the baseline machine.
#   * Important: the host .so are bound by the glibc baseline - run this on the oldest distribution
#     you need to support, otherwise older machines fail to load them due to glibc symbol versions.
# ============================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# -- system sources (overridable via environment variables) --
SYS_OPENSSL_INC="${SYS_OPENSSL_INC:-/usr/include/openssl}"               # public headers (excluding opensslconf.h)
SYS_OPENSSLCONF_X86="${SYS_OPENSSLCONF_X86:-/usr/include/x86_64-linux-gnu/openssl/opensslconf.h}"
SYS_OPENSSLCONF_ARM="${SYS_OPENSSLCONF_ARM:-/usr/include/aarch64-linux-gnu/include/aarch64-linux-gnu/openssl/opensslconf.h}"
SYS_MOSQUITTO_H="${SYS_MOSQUITTO_H:-/usr/include/mosquitto.h}"
SYS_LIBDIR_X86="${SYS_LIBDIR_X86:-/usr/lib/x86_64-linux-gnu}"

die(){ echo "ERROR: $*" >&2; exit 1; }

copy_openssl_headers() {
    local dst="$1" conf="$2"
    [ -d "$SYS_OPENSSL_INC" ] || die "OpenSSL header dir not found: $SYS_OPENSSL_INC (install libssl-dev)"
    [ -f "$conf" ]            || die "opensslconf.h not found: $conf"
    mkdir -p "$dst/openssl"
    # public headers are arch-independent
    cp -f "$SYS_OPENSSL_INC"/*.h "$dst/openssl/"
    # only opensslconf.h is arch-specific (word size / RC4_INT); override it per arch
    cp -f "$conf" "$dst/openssl/opensslconf.h"
    echo "  openssl headers -> $dst/openssl ($(ls "$dst/openssl" | wc -l) files, conf=$conf)"
}

echo "[vendor-deps] OpenSSL 1.1.1 / libmosquitto 1.6.9"

# -- x86_64-linux-gnu (host, fully bundled) --
X86="$HERE/x86_64-linux-gnu"
echo "[x86_64-linux-gnu]"
copy_openssl_headers "$X86/include" "$SYS_OPENSSLCONF_X86"
[ -f "$SYS_MOSQUITTO_H" ] || die "mosquitto.h not found: $SYS_MOSQUITTO_H (install libmosquitto-dev)"
cp -f "$SYS_MOSQUITTO_H" "$X86/include/mosquitto.h"
echo "  mosquitto.h -> $X86/include/mosquitto.h"
mkdir -p "$X86/lib"
# copy the real .so files and recreate the version symlinks (copying symlinks could leave them dangling)
for real in libssl.so.1.1 libcrypto.so.1.1 libmosquitto.so.1.6.9; do
    [ -f "$SYS_LIBDIR_X86/$real" ] || die "not found: $SYS_LIBDIR_X86/$real"
    cp -f "$SYS_LIBDIR_X86/$real" "$X86/lib/$real"
done
( cd "$X86/lib"
  ln -sf libssl.so.1.1        libssl.so
  ln -sf libcrypto.so.1.1     libcrypto.so
  ln -sf libmosquitto.so.1.6.9 libmosquitto.so.1
  ln -sf libmosquitto.so.1.6.9 libmosquitto.so )
echo "  libs -> $X86/lib ($(ls "$X86/lib" | wc -l) entries)"

# -- aarch64-linux-gnu (target board: .so bundled, only refresh OpenSSL headers) --
ARM="$HERE/aarch64-linux-gnu"
echo "[aarch64-linux-gnu]"
if [ ! -f "$SYS_OPENSSLCONF_ARM" ]; then
    echo "  WARN: arm64 opensslconf.h not found ($SYS_OPENSSLCONF_ARM)."
    echo "        install libssl-dev:arm64, or set SYS_OPENSSLCONF_ARM=/path and re-run."
else
    copy_openssl_headers "$ARM/include" "$SYS_OPENSSLCONF_ARM"
fi
[ -f "$ARM/include/mosquitto.h" ] && echo "  mosquitto.h: bundled (kept)"
[ -f "$ARM/lib/libmosquitto.so" ] && echo "  libs: aarch64 prebuilt .so bundled (kept; cannot be regenerated on host)"

echo "[vendor-deps] done."
