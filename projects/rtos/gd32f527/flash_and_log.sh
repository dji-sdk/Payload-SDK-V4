#!/bin/bash
# Flash the GD32F527 application only. The bootloader region is untouched.
#   ./flash_and_log.sh [log_seconds] [bin_path]
# bin_path defaults to the single build_gd32/<sn>_V<version>.bin artifact.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_SEC="${1:-90}"
BIN="${2:-}"
if [ -z "${BIN}" ]; then
    # pick the single <sn>_V<version>.bin the build produced
    BIN=$(ls -t "${SCRIPT_DIR}/build_gd32/"*_V*.bin 2>/dev/null | head -1)
fi
SERIAL="/dev/ttyUSB0"

if [ -z "${BIN}" ] || [ ! -f "${BIN}" ]; then
    echo "error: firmware bin not found (looked for build_gd32/*_V*.bin)" >&2
    echo "  build the firmware first, or pass the bin path as arg 2 (e.g. build_gd32/payload_iot_demo.bin)" >&2
    exit 1
fi
echo "[flash] using ${BIN}"

cat > /tmp/gd32f5.cfg <<'EOF'
source [find interface/stlink.cfg]
transport select hla_swd
adapter speed 1000
set _CHIPNAME gd32f5
swd newdap $_CHIPNAME cpu -expected-id 0x0be12477
dap create $_CHIPNAME.dap -chain-position $_CHIPNAME.cpu
target create $_CHIPNAME.cpu cortex_m -dap $_CHIPNAME.dap
EOF

cat > /tmp/gd32f5_flash.tcl <<'EOF'
set FMC_KEY  0x40023C04
set FMC_STAT 0x40023C0C
set FMC_CTL  0x40023C10
set APP_ADDR 0x08020000

proc fmc_read {addr} {
    return [lindex [read_memory $addr 32 1] 0]
}

proc fmc_wait {} {
    set timeout 100000
    while {([fmc_read $::FMC_STAT] & 0x10000) != 0} {
        incr timeout -1
        if {$timeout == 0} { error "FMC busy timeout" }
    }
    set status [fmc_read $::FMC_STAT]
    if {($status & 0xF2) != 0} { error [format "FMC error status=0x%08X" $status] }
    mww $::FMC_STAT 0xF3
}

proc fmc_unlock {} {
    mww $::FMC_KEY 0x45670123
    mww $::FMC_KEY 0xCDEF89AB
}

proc fmc_erase_sector {sector} {
    set ctl [expr {0x2 | (($sector & 0x1f) << 3)}]
    if {$sector >= 32} { set ctl [expr {$ctl | 0x800}] }
    mww $::FMC_CTL $ctl
    mww $::FMC_CTL [expr {$ctl | 0x10000}]
    fmc_wait
    mww $::FMC_CTL 0
}

proc gd32_flash_app {binfile} {
    set bytes [file size $binfile]
    if {$bytes > 0x40000} { error "app is larger than reserved sectors 5-6" }
    if {($bytes & 7) != 0} { error "app binary must be 8-byte aligned" }

    echo [format "Erasing app sectors 5-6 (0x08020000..0x0805FFFF), %d bytes" $bytes]
    fmc_unlock
    fmc_wait
    fmc_erase_sector 5
    fmc_erase_sector 6

    echo "Programming app with 64-bit FMC writes"
    mww $::FMC_CTL 0x401
    set fd [open $binfile rb]
    fconfigure $fd -translation binary
    set addr $::APP_ADDR
    set count 0
    while {1} {
        set chunk [read $fd 8]
        if {[string length $chunk] == 0} { break }
        binary scan $chunk ii w0 w1
        mww $addr [format "0x%08X" [expr {$w0 & 0xffffffff}]]
        mww [expr {$addr + 4}] [format "0x%08X" [expr {$w1 & 0xffffffff}]]
        fmc_wait
        incr count
        if {($count % 4096) == 0} { echo [format "  %d / %d KiB" [expr {$count / 128}] [expr {$bytes / 1024}]] }
        set addr [expr {$addr + 8}]
    }
    close $fd
    mww $::FMC_CTL 0x80000000
    echo "FMC programming complete"
}
EOF

python3 - "${BIN}" > /tmp/gd32f5_program.tcl <<'PY'
import pathlib
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if len(data) > 0x40000:
    raise SystemExit("application exceeds sectors 5-6")
if len(data) % 8:
    raise SystemExit("application is not 8-byte aligned")

print(f'echo "Erasing app sectors 5-6 (0x08020000..0x0805FFFF), {len(data)} bytes"')
print('fmc_unlock')
print('fmc_wait')
print('fmc_erase_sector 5')
print('fmc_erase_sector 6')
print('echo "Programming app with 64-bit FMC writes"')
print('mww 0x40023C10 0x401')
for offset in range(0, len(data), 8):
    w0 = int.from_bytes(data[offset:offset + 4], 'little')
    w1 = int.from_bytes(data[offset + 4:offset + 8], 'little')
    addr = 0x08020000 + offset
    print(f'mww 0x{addr:08X} 0x{w0:08X}')
    print(f'mww 0x{addr + 4:08X} 0x{w1:08X}')
    print('sleep 20')
    if (offset + 8) % 32768 == 0:
        print(f'echo "  programmed {(offset + 8) // 1024} KiB"')
print('mww 0x40023C10 0x80000000')
print('echo "FMC programming complete"')
PY

echo "=== [1/3] Flashing app to 0x08020000 ==="
openocd -f /tmp/gd32f5.cfg \
    -c "source /tmp/gd32f5_flash.tcl" \
    -c "init" \
    -c "reset halt" \
    -c "wait_halt 1000" \
    -c "poll off" \
    -c "source /tmp/gd32f5_program.tcl" \
    -c "verify_image ${BIN} 0x08020000" \
    -c "poll on" \
    -c "reset run" \
    -c "shutdown"

echo "=== [2/3] Capturing ${LOG_SEC}s from ${SERIAL} ==="
timeout "${LOG_SEC}" cat "${SERIAL}" || true
echo "=== [3/3] Done ==="
