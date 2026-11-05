## Source
- Repository: https://github.com/Mbed-TLS/mbedtls
- Version: v2.28.9 (LTS branch)
- Download: https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v2.28.9.tar.gz
- License: Apache 2.0
## Purpose
Provides TLS 1.2 stack for GD32F527 platform, enabling PSDK V4 MQTT secure access:
- TLS client connection to broker:8883, EKM key export (RFC 5705)
- HMAC-SHA256 message signing/verification
- Integrated with lwIP MQTT client via altcp_tls adapter
## Directory Structure
mbedtls-2.28.9/
├── include/mbedtls/    # Headers (kept intact)
├── library/            # Source files (kept intact, CMakeLists selects which to compile)
├── LICENSE             # Apache 2.0 license
└── README.md           # This file
## Platform Adaptation (external)
Platform-specific files are located in `projects/rtos/gd32f527/network/tls/`:
- `mbedtls_config_gd32.h` — Build configuration (specified via `-DMBEDTLS_CONFIG_FILE`)
- `mbedtls_hw_rng.c` — GD32F527 TRNG hardware entropy source
- `ca_cert.h` — DJI MQTT broker CA certificate (PEM embedded as C array)
## Why 2.28.x (not 3.x)
- lwIP `altcp_tls_mbedtls.c` requires `mbedtls/ssl_internal.h`, removed in 3.x
- EKM export API `mbedtls_ssl_conf_export_keys_ext_cb()` removed in 3.x
- 2.28 is the Long Term Support branch
## Build Configuration Summary
Minimal feature set for secure access only:
- TLS 1.2 client (no server, no DTLS, no TLS 1.0/1.1)
- RSA + AES-GCM cipher suites
- SHA-256 (HMAC signing)
- X.509 certificate parsing (CA verification)
- EKM key export (`MBEDTLS_SSL_EXPORT_KEYS`)
- No filesystem I/O (certificates embedded as C arrays)
- Memory allocation via FreeRTOS pvPortMalloc/vPortFree
## Notes
1. All source files under `library/` are preserved, but only ~31 files are explicitly compiled via CMakeLists.txt. Unused files do not increase firmware size.
2. ARM GCC newlib-nano does not support `%llu` (64-bit printf). Avoid in any mbedTLS-related logging.
3. TLS handshake peak memory is ~30-40 KB. Ensure lwIP `MEM_SIZE` and FreeRTOS heap are sufficient.
4. `MBEDTLS_SSL_MAX_CONTENT_LEN` is set to 4096 (default 16384) to reduce memory usage.