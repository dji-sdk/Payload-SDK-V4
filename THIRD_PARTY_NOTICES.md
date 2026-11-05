# Third-Party Notices

This project includes or depends on the following third-party components. Copyright of each component belongs to its original authors.

## Distributed with source (vendored)

### Linux samples

| Component | Version | License | Location |
|---|---|---|---|
| cJSON | 1.7.19 | MIT | samples/third_party/cJSON/ |
| Mongoose | 7.19 | GPL-2.0 / commercial dual-license | samples/third_party/mongoose/ |
| OpenSSL (libssl, libcrypto) | 1.1.1 | Apache-2.0 / OpenSSL License | samples/third_party/&lt;triplet&gt;/lib/ |
| Eclipse Mosquitto (libmosquitto) | 1.6.9 | EPL-2.0 / EDL-1.0 | samples/third_party/&lt;triplet&gt;/lib/ |

OpenSSL/mosquitto are distributed as **precompiled shared libraries (.so) + headers** with the package (split by GNU triplet into directories,
x86_64-linux-gnu / aarch64-linux-gnu), so SDK build and run do not depend on target-system libraries.

### GD32F527 RTOS project

| Component | Version | License | Location |
|---|---|---|---|
| FreeRTOS Kernel | 10.3.1 | MIT | projects/rtos/gd32f527/middlewares/FreeRTOS-10.3.1/ |
| lwIP | 2.1.2 | BSD-3-Clause | projects/rtos/gd32f527/middlewares/lwip-2.1.2/ |
| Mbed TLS | 2.28.9 | Apache-2.0 / GPL-2.0-or-later | projects/rtos/gd32f527/middlewares/mbedtls-2.28.9/ |
| Arm CMSIS | — | Apache-2.0 | projects/rtos/gd32f527/bsp/drivers/CMSIS/ |
| GD32F5xx firmware library (standard peripheral + USB) | 2023-12-20 | BSD-3-Clause (GigaDevice) | projects/rtos/gd32f527/bsp/drivers/ |

License texts are kept in each component's directory (LICENSE / COPYING) or in the header of its source files.
