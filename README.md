# Payload SDK for IoT — Things-Model MQTT SDK

A C SDK for building DJI payload devices that speak the Things-Model MQTT protocol (V1 / V2).
The package is self-contained: `sdk/` carries the protocol code plus a minimal C runtime, so a
plain `cmake && make` builds everything with no extra tooling.

## Directory structure

```
sdk/
  generated/c/      protocol code, per capability domain (widget, power, upgrade, …)
  runtime/c/        minimal C runtime (session + auth + MQTT transport; host + rtos backends)
samples/            end-to-end C samples and the shared launcher
  launcher/            sample registration, session, device-info reporting
  power/ … widget/    per-capability sample sources
  third_party/         bundled cJSON + precompiled OpenSSL / Mosquitto
projects/linux/          Linux host / aarch64 single-binary sample project
projects/rtos/gd32f527/  GD32F527 FreeRTOS / lwIP / mbedTLS project
LICENSE                  MIT
THIRD_PARTY_NOTICES.md   third-party component license notices
```

## 1. Build dependencies

| Platform | Command |
|---|---|
| Ubuntu / Debian | `sudo apt install build-essential cmake libssl-dev libmosquitto-dev` |
| RHEL / CentOS / Fedora | `sudo dnf install gcc cmake openssl-devel mosquitto-devel` |
| macOS (Homebrew) | `brew install cmake mosquitto openssl` |

cJSON needs no install — the samples use the bundled `samples/third_party/cJSON/`. Requires `cmake ≥ 3.13`.

## 2. Build and run the samples (Linux host)

All samples (efts / power / time_sync / upgrade / widget …) are combined into a single executable
`sample_things_model`, selected via a menu or a command argument.

```bash
cmake -B build_host -S projects/linux -DSAMPLE_BUILD=host
cmake --build build_host -j
ls build_host/sample_things_model
```

Running needs an MQTT broker and your access credentials (app_key / app_license, etc.). Provide them
either way (same-name priority: env var > local header > placeholder default):

**A — local header (compile-time, recommended)**

```bash
cd projects/linux
cp sample_config_local.h.example sample_config_local.h   # once
$EDITOR sample_config_local.h        # fill in app_key / app_license / broker …
# sample_config_local.h is gitignored — never committed, never packaged
```

**B — environment variables (run-time)**

```bash
export THING_MODEL_BROKER=ssl://your-broker:8883
export THING_MODEL_APP_ID=...        THING_MODEL_APP_NAME=...
export THING_MODEL_APP_KEY=...       THING_MODEL_APP_LICENSE=...
export THING_MODEL_DEVICE_SN=...     THING_MODEL_CA_CERT_PATH=/path/to/ca.crt
```

Run:

```bash
cd build_host
./sample_things_model                 # interactive menu
./sample_things_model power -v        # run one sample directly
./sample_things_model --list          # list samples (script-friendly)
./sample_things_model --help          # overall usage
```

Zero-input autorun (compile-time selection, no console needed):

```bash
cmake -B build_host -S projects/linux -DSAMPLE_BUILD=host -DSAMPLE_AUTORUN=power
cmake --build build_host -j
# or run all samples in turn, with an optional per-sample time budget and loop:
cmake -B build_host -S projects/linux -DSAMPLE_BUILD=host \
      -DSAMPLE_AUTORUN=all -DSAMPLE_AUTORUN_BUDGET=20 -DSAMPLE_AUTORUN_LOOP=ON
cmake --build build_host -j
```

## 3. Cross-compile (aarch64)

```bash
sudo apt install gcc-aarch64-linux-gnu
cmake -B build_aarch64 -S projects/linux -DSAMPLE_BUILD=cross
cmake --build build_aarch64 -j
file build_aarch64/sample_things_model     # ARM aarch64
```

## 4. GD32F527 RTOS firmware

```bash
sudo apt install gcc-arm-none-eabi
cp projects/rtos/gd32f527/app/sample_config_local.h.example \
   projects/rtos/gd32f527/app/sample_config_local.h
$EDITOR projects/rtos/gd32f527/app/sample_config_local.h
cmake -B build_gd32 -S projects/rtos/gd32f527 \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/projects/rtos/gd32f527/toolchain-arm-none-eabi.cmake
cmake --build build_gd32          # single-threaded (project constraint; do not add -j)
```

Artifacts in `build_gd32/`: `payload_iot_demo.elf` (debug symbols), `payload_iot_demo.bin`
(flash at `0x08020000`), `payload_iot_demo.hex`.

Flash via ST-Link (requires `openocd`), then watch the serial log (921600 baud):

```bash
# args: [log_seconds] [bin_path]; flashes the application region only, then captures the serial log
projects/rtos/gd32f527/flash_and_log.sh 90 build_gd32/payload_iot_demo.bin
# or watch the serial log manually:
stty -F /dev/ttyUSB0 921600 cs8 -cstopb -parenb -crtscts && cat /dev/ttyUSB0
```

## 5. Using the SDK in your own project

Use `sdk/generated/c` (protocol code) plus `sdk/runtime/c` (runtime source) directly:
`add_subdirectory(sdk/runtime/c)` and link — no extra tooling required.

## 6. Third-party dependencies

The C runtime's OpenSSL / Mosquitto ship as precompiled `.so` + headers under
`samples/third_party/<triplet>/` (x86_64-linux-gnu / aarch64-linux-gnu), so building and running do
not depend on target-system libraries. Licenses in `THIRD_PARTY_NOTICES.md`.

| Component | Purpose | License |
|---|---|---|
| OpenSSL (libssl/libcrypto) | C runtime TLS + HMAC | Apache-2.0 / OpenSSL |
| Eclipse Mosquitto (libmosquitto) | C runtime MQTT transport | EPL-2.0 / EDL-1.0 |

## License

Released under the [MIT License](LICENSE). Third-party licenses in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
