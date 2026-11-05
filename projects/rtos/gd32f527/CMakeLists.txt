# ============================================================
# projects/rtos/gd32f527/CMakeLists.txt
# Things-Model SDK — GD32F527 + FreeRTOS + lwIP + mbedTLS sample
#
# Goal:
#   Connect to the broker, run two-phase authentication (Phase1 TLS +
#   Phase2 HMAC), and keep the connection alive with heartbeats.
#   No business features here — business samples are layered on later.
#
# Usage:
#   cmake -S projects/rtos/gd32f527 \
#         -B build_gd32 \
#         -DCMAKE_TOOLCHAIN_FILE=projects/rtos/gd32f527/toolchain-arm-none-eabi.cmake
#   cmake --build build_gd32              # single-threaded build, per project convention
#   arm-none-eabi-objcopy -O binary build_gd32/payload_iot_demo.elf \
#       build_gd32/payload_iot_demo.bin
#
# Required:
#   Credentials such as USER_APP_ID/KEY/LICENSE/DEVICE_SN/BROKER live in
#   app/sample_config_local.h, which is committed directly (unlike the
#   Linux side, where it is a template) — see app/README and
#   app/sample_config_local.h.
# ============================================================
cmake_minimum_required(VERSION 3.15)

# The toolchain is already loaded via -DCMAKE_TOOLCHAIN_FILE; if the user
# forgot to pass it, fail early here.
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "Please use the toolchain file:\n"
        "  cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_LIST_DIR}/toolchain-arm-none-eabi.cmake \\\n"
        "        -S ${CMAKE_CURRENT_LIST_DIR} -B build_gd32")
endif()

project(payload_iot_demo C ASM)

# ============================================================
# Paths
# ============================================================
set(SDK_ROOT    ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(SAMPLE_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(SAMPLE_DEVICE_FIRMWARE_VERSION "01.00.09.08" CACHE STRING "Device firmware version")
# Device SN == device name (single source). Override with -DSAMPLE_DEVICE_SN=<sn>.
set(SAMPLE_DEVICE_SN "gd32_test" CACHE STRING "Device SN (also used as device name)")

# License profile: selects which credential set app/sample_config_local.h compiles in
# (empty = the default credentials in that local header).
set(SAMPLE_LICENSE_PROFILE "" CACHE STRING "License profile (empty = local default)")

# Protocol code + runtime sources come from the bundled sdk/ snapshot.
set(_PSDK_SNAPSHOT ${SDK_ROOT}/sdk)
if(EXISTS ${_PSDK_SNAPSHOT}/generated)
    set(PAYLOAD_RELEASE_MODE ON)
else()
    set(PAYLOAD_RELEASE_MODE OFF)
endif()

# ============================================================
# Middleware subdirectory paths — prefilled for the runtime subproject
# ============================================================
set(_FREERTOS_DIR ${SAMPLE_DIR}/middlewares/FreeRTOS-10.3.1)
set(_LWIP_DIR     ${SAMPLE_DIR}/middlewares/lwip-2.1.2)
set(_MBEDTLS_DIR  ${SAMPLE_DIR}/middlewares/mbedtls-2.28.9)

# Include-path lists (consumed by runtime/c/CMakeLists.txt) — must be set
# before add_subdirectory.
set(FREERTOS_INCLUDE_DIRS
    ${_FREERTOS_DIR}/include
    ${_FREERTOS_DIR}/portable/GCC/ARM_CM33_NTZ
    ${_FREERTOS_DIR}/portable/GCC/ARM_CM33_NTZ/non_secure
    ${SAMPLE_DIR}/bsp/board                # FreeRTOSConfig.h
    CACHE STRING "" FORCE)

set(LWIP_INCLUDE_DIRS
    ${_LWIP_DIR}/src/include
    ${_LWIP_DIR}/src/apps/altcp_tls
    ${_LWIP_DIR}/port/GD32F527
    ${_LWIP_DIR}/port/GD32F527/Basic
    ${_LWIP_DIR}/port/GD32F527/FreeRTOS              # sys_arch.h
    ${SAMPLE_DIR}/network                  # lwipopts.h
    CACHE STRING "" FORCE)

set(MBEDTLS_INCLUDE_DIRS
    ${_MBEDTLS_DIR}/include
    ${SAMPLE_DIR}/network/tls              # mbedtls_config_gd32.h
    CACHE STRING "" FORCE)

# cJSON is built from source — the runtime compiles cJSON.c into
# thing_model_client_runtime. Reuse the host-side vendored cJSON copy
# (newer version, provides cJSON_ParseWithLength, which the runtime needs).
set(CJSON_SRC ${SDK_ROOT}/samples/third_party/cJSON/cJSON.c CACHE FILEPATH "" FORCE)
set(CJSON_INC ${SDK_ROOT}/samples/third_party/cJSON         CACHE PATH     "" FORCE)

# ============================================================
# Global compile definitions (needed by BSP / mbedTLS / sample)
# ============================================================
add_compile_definitions(
    GD32F527
    USE_HAL_DRIVER
    USE_USB_FS
    USE_USB_RNDIS
    USE_BOOTLOADER                       # Critical! Tells SystemInit to set VTOR to 0x08020000
                                          # (defined in bsp/drivers/CMSIS/GD/GD32F5xx/Source/system_gd32f5xx.c
                                          #  via VECT_TAB_OFFSET = 0x20000)
                                          # If undefined -> VTOR=0x08000000 -> HardFault lockup on the first interrupt
    ARM_MATH_CM33
    ARM_MATH_MATRIX_CHECK
    ARM_MATH_ROUNDING
    MBEDTLS_CONFIG_FILE="mbedtls_config_gd32.h"
    THING_MODEL_OSAL_FREERTOS
    SAMPLE_DEVICE_NAME="${SAMPLE_DEVICE_SN}"
    SAMPLE_DEVICE_SN="${SAMPLE_DEVICE_SN}"
    SAMPLE_DEVICE_FIRMWARE_VERSION="${SAMPLE_DEVICE_FIRMWARE_VERSION}"
)

# License profile → compile-time selector consumed by app/sample_config_local.h.
if(SAMPLE_LICENSE_PROFILE)
    add_compile_definitions("SAMPLE_LICENSE_PROFILE_${SAMPLE_LICENSE_PROFILE}=1")
endif()

# Debug/Release optimization (defaults to Debug; running Release is not
# recommended for the sample)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    add_compile_options(-Ofast)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    add_compile_options(-Os)
else()
    add_compile_options(-Og -g)
endif()

# ============================================================
# Pull in the runtime — rtos platform + uav profile; three targets are
# created automatically (built from runtime/c in the sdk/ snapshot).
# ============================================================
set(THING_MODEL_PLATFORM rtos CACHE STRING "" FORCE)
set(THING_MODEL_PROFILE  uav  CACHE STRING "" FORCE)
if(PAYLOAD_RELEASE_MODE)
    add_subdirectory(${_PSDK_SNAPSHOT}/runtime/c ${CMAKE_BINARY_DIR}/_thing_model_runtime)
else()
    message(FATAL_ERROR "This package builds from the bundled sdk/ protocol code; the development toolchain is not included.")
endif()

# ============================================================
# BSP — GD32 standard peripheral library + CMSIS + USB
# ============================================================
file(GLOB _BSP_DRV_SRC
    ${SAMPLE_DIR}/bsp/drivers/BSP/*.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_standard_peripheral/Source/*.c
    ${SAMPLE_DIR}/bsp/drivers/CMSIS/GD/GD32F5xx/Source/syscalls.c
    ${SAMPLE_DIR}/bsp/drivers/CMSIS/GD/GD32F5xx/Source/system_gd32f5xx.c
    ${SAMPLE_DIR}/bsp/hal/*.c
    # DJI Osal_*/dji_platform shim — a thin FreeRTOS wrapper the BSP/USB
    # stack depends on heavily. Not used by runtime/c (the runtime uses its
    # own T_ThingModelOsal vtable); BSP only.
    ${SAMPLE_DIR}/bsp/board/osal.c
    # Cortex-M33 fault dump (HardFault/MemManage/BusFault/UsageFault).
    # Overrides the weak aliases; at link time the strong symbols replace
    # the default dead-loop handlers in startup.
    # See bsp/board/fault_handler.h.
    ${SAMPLE_DIR}/bsp/board/fault_handler.c
)

# USB device core + RNDIS class
list(APPEND _BSP_DRV_SRC
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/driver/Source/drv_usb_core.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/driver/Source/drv_usb_dev.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/driver/Source/drv_usbd_int.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/core/Source/usbd_core.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/core/Source/usbd_enum.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/core/Source/usbd_transc.c
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/class/rndis/Source/rndis_core.c
)

add_library(bsp STATIC ${_BSP_DRV_SRC})
target_include_directories(bsp PUBLIC
    ${SAMPLE_DIR}/bsp/drivers/CMSIS/include
    ${SAMPLE_DIR}/bsp/drivers/CMSIS/GD/GD32F5xx/Include
    ${SAMPLE_DIR}/bsp/drivers/BSP
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_standard_peripheral/Include
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/core/Include
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/driver/Include
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/device/class/rndis/Include
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/ustd/class/rndis
    ${SAMPLE_DIR}/bsp/drivers/GD32F5xx_usb_library/ustd/common
    ${SAMPLE_DIR}/bsp/hal
    ${SAMPLE_DIR}/bsp/board                  # FreeRTOSConfig.h + DJI shim (dji_platform.h)
    ${FREERTOS_INCLUDE_DIRS}                 # osal.c shim uses FreeRTOS.h
    ${SAMPLE_DIR}/network                    # usb_device.c uses netconf.h
    ${SAMPLE_DIR}/network/rndis              # usb_rndis_config.h
    ${LWIP_INCLUDE_DIRS}                     # netconf.h transitively needs lwip/netif.h
    ${SDK_ROOT}/samples/time_sync/src
    ${SDK_ROOT}/samples/efts/src
)

# ============================================================
# FreeRTOS — kernel + ARM_CM33_NTZ port + heap_4
# ============================================================
add_library(freertos STATIC
    ${_FREERTOS_DIR}/croutine.c
    ${_FREERTOS_DIR}/event_groups.c
    ${_FREERTOS_DIR}/list.c
    ${_FREERTOS_DIR}/queue.c
    ${_FREERTOS_DIR}/tasks.c
    ${_FREERTOS_DIR}/stream_buffer.c
    ${_FREERTOS_DIR}/timers.c
    ${_FREERTOS_DIR}/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
    ${_FREERTOS_DIR}/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
    ${_FREERTOS_DIR}/portable/MemMang/heap_4.c
)
target_include_directories(freertos PUBLIC ${FREERTOS_INCLUDE_DIRS})
target_link_libraries(freertos PUBLIC bsp)  # the port needs CMSIS

# ============================================================
# lwIP — core + ipv4 + apps/mqtt + altcp + altcp_tls(mbedtls)
# ============================================================
add_library(lwip STATIC
    # core
    ${_LWIP_DIR}/src/core/init.c
    ${_LWIP_DIR}/src/core/def.c
    ${_LWIP_DIR}/src/core/mem.c
    ${_LWIP_DIR}/src/core/memp.c
    ${_LWIP_DIR}/src/core/netif.c
    ${_LWIP_DIR}/src/core/pbuf.c
    ${_LWIP_DIR}/src/core/tcp.c
    ${_LWIP_DIR}/src/core/tcp_in.c
    ${_LWIP_DIR}/src/core/tcp_out.c
    ${_LWIP_DIR}/src/core/udp.c
    ${_LWIP_DIR}/src/core/ip.c
    ${_LWIP_DIR}/src/core/timeouts.c
    ${_LWIP_DIR}/src/core/sys.c
    ${_LWIP_DIR}/src/core/inet_chksum.c
    ${_LWIP_DIR}/src/core/stats.c
    # altcp
    ${_LWIP_DIR}/src/core/altcp.c
    ${_LWIP_DIR}/src/core/altcp_alloc.c
    ${_LWIP_DIR}/src/core/altcp_tcp.c
    # ipv4
    ${_LWIP_DIR}/src/core/ipv4/ip4.c
    ${_LWIP_DIR}/src/core/ipv4/ip4_addr.c
    ${_LWIP_DIR}/src/core/ipv4/ip4_frag.c
    ${_LWIP_DIR}/src/core/ipv4/etharp.c
    ${_LWIP_DIR}/src/core/ipv4/icmp.c
    ${_LWIP_DIR}/src/core/ipv4/dhcp.c
    # netif
    ${_LWIP_DIR}/src/netif/ethernet.c
    # apps
    ${_LWIP_DIR}/src/apps/mqtt/mqtt.c
    ${_LWIP_DIR}/src/apps/http/httpd.c
    ${_LWIP_DIR}/src/apps/http/fs.c
    ${_LWIP_DIR}/src/apps/altcp_tls/altcp_tls_mbedtls.c
    ${_LWIP_DIR}/src/apps/altcp_tls/altcp_tls_mbedtls_mem.c
    # api — required for NO_SYS=0: tcpip_thread + tcpip_input + err helpers.
    # We do NOT pull in api_lib/api_msg/sockets/netbuf/netdb/if_api/netifapi:
    # LWIP_NETCONN / LWIP_SOCKET are disabled; traffic goes over raw mqtt + altcp.
    ${_LWIP_DIR}/src/api/tcpip.c
    ${_LWIP_DIR}/src/api/err.c
    # port — GD32 ethernetif over RNDIS
    ${_LWIP_DIR}/port/GD32F527/Basic/ethernetif_rndis.c
    # port — FreeRTOS sys_arch (NO_SYS=0 mutex/sem/mbox/thread + core lock)
    ${_LWIP_DIR}/port/GD32F527/FreeRTOS/sys_arch.c
)
target_include_directories(lwip PUBLIC ${LWIP_INCLUDE_DIRS})
# sys_arch.c needs FreeRTOS.h/task.h/semphr.h;
# altcp_tls_mbedtls.c needs mbedtls/ssl.h
target_link_libraries(lwip PUBLIC freertos bsp)
target_include_directories(lwip PRIVATE
    ${MBEDTLS_INCLUDE_DIRS}
    ${FREERTOS_INCLUDE_DIRS}
)

# ============================================================
# mbedTLS — TLS 1.2 client, EKM, HMAC-SHA256, CTR_DRBG
# ============================================================
add_library(mbedtls STATIC
    ${_MBEDTLS_DIR}/library/aes.c
    ${_MBEDTLS_DIR}/library/asn1parse.c
    ${_MBEDTLS_DIR}/library/asn1write.c
    ${_MBEDTLS_DIR}/library/base64.c
    ${_MBEDTLS_DIR}/library/bignum.c
    ${_MBEDTLS_DIR}/library/cipher.c
    ${_MBEDTLS_DIR}/library/cipher_wrap.c
    ${_MBEDTLS_DIR}/library/constant_time.c
    ${_MBEDTLS_DIR}/library/ctr_drbg.c
    ${_MBEDTLS_DIR}/library/entropy.c
    ${_MBEDTLS_DIR}/library/entropy_poll.c
    ${_MBEDTLS_DIR}/library/error.c
    ${_MBEDTLS_DIR}/library/gcm.c
    ${_MBEDTLS_DIR}/library/md.c
    ${_MBEDTLS_DIR}/library/md5.c
    ${_MBEDTLS_DIR}/library/oid.c
    ${_MBEDTLS_DIR}/library/pem.c
    ${_MBEDTLS_DIR}/library/pk.c
    ${_MBEDTLS_DIR}/library/pk_wrap.c
    ${_MBEDTLS_DIR}/library/pkparse.c
    ${_MBEDTLS_DIR}/library/platform.c
    ${_MBEDTLS_DIR}/library/platform_util.c
    ${_MBEDTLS_DIR}/library/rsa.c
    ${_MBEDTLS_DIR}/library/rsa_internal.c
    ${_MBEDTLS_DIR}/library/sha256.c
    ${_MBEDTLS_DIR}/library/sha512.c
    ${_MBEDTLS_DIR}/library/ssl_ciphersuites.c
    ${_MBEDTLS_DIR}/library/ssl_cli.c
    ${_MBEDTLS_DIR}/library/ssl_msg.c
    ${_MBEDTLS_DIR}/library/ssl_tls.c
    ${_MBEDTLS_DIR}/library/x509.c
    ${_MBEDTLS_DIR}/library/x509_crt.c
    ${SAMPLE_DIR}/network/tls/mbedtls_hw_rng.c   # GD32 TRNG entropy
)
target_include_directories(mbedtls PUBLIC ${MBEDTLS_INCLUDE_DIRS})
target_link_libraries(mbedtls PUBLIC bsp)

# ============================================================
# Network init — RNDIS + lwIP netif config
# ============================================================
add_library(network STATIC
    ${SAMPLE_DIR}/network/netconf.c
)
target_include_directories(network PUBLIC
    ${SAMPLE_DIR}/network
    ${SAMPLE_DIR}/network/rndis
    ${SAMPLE_DIR}/network/tls
)
target_link_libraries(network PUBLIC lwip bsp)

# ============================================================
# Generated code — prebuilt .c/.h files taken from the sdk/ snapshot's
#   generated/c directory (no code generation runs during this build).
# ============================================================
if(PAYLOAD_RELEASE_MODE)
    set(_GEN ${_PSDK_SNAPSHOT}/generated/c)
else()
    set(_GEN ${SDK_ROOT}/build/gen_rtos/c)
endif()

# Helper: create a generated-code static library for one cap
function(_tm_gen_lib name)
    add_library(thing_model_gen_${name} STATIC ${ARGN})
    target_include_directories(thing_model_gen_${name} PUBLIC ${_GEN})
    target_link_libraries(thing_model_gen_${name} PUBLIC thing_model_client_runtime thing_model_gen_common)
endfunction()

# common (depended on by every cap)
add_library(thing_model_gen_common STATIC ${_GEN}/common/common_json.c)
target_include_directories(thing_model_gen_common PUBLIC ${_GEN})
target_link_libraries(thing_model_gen_common PUBLIC thing_model_client_runtime)

# business caps
_tm_gen_lib(custom_small_data
    ${_GEN}/custom_small_data/custom_small_data_client.c
    ${_GEN}/custom_small_data/custom_small_data_json.c
    ${_GEN}/custom_small_data/custom_small_data_publisher.c)

_tm_gen_lib(data_subscription
    ${_GEN}/data_subscription/data_subscription_client.c
    ${_GEN}/data_subscription/data_subscription_json.c
    ${_GEN}/data_subscription/data_subscription_publisher.c)


_tm_gen_lib(power
    ${_GEN}/power/power_client.c
    ${_GEN}/power/power_json.c
    ${_GEN}/power/power_publisher.c
    ${_GEN}/power/power_impl.c)

_tm_gen_lib(efts
    ${_GEN}/efts/efts_client.c
    ${_GEN}/efts/efts_json.c
    ${_GEN}/efts/efts_publisher.c)

_tm_gen_lib(time_sync
    ${_GEN}/time_sync/time_sync_client.c
    ${_GEN}/time_sync/time_sync_json.c
    ${_GEN}/time_sync/time_sync_publisher.c)

_tm_gen_lib(upgrade
    ${_GEN}/upgrade/upgrade_client.c
    ${_GEN}/upgrade/upgrade_json.c
    ${_GEN}/upgrade/upgrade_publisher.c
    ${_GEN}/upgrade/upgrade_impl.c)

_tm_gen_lib(widget
    ${_GEN}/widget/widget_client.c
    ${_GEN}/widget/widget_json.c
    ${_GEN}/widget/widget_publisher.c
    ${_GEN}/widget/widget_impl.c)

_tm_gen_lib(speaker
    ${_GEN}/speaker/speaker_impl.c
    ${_GEN}/speaker/speaker_json.c
    ${_GEN}/speaker/speaker_publisher.c)

# launcher-support caps (required by sample_session)
_tm_gen_lib(aircraft_info  ${_GEN}/aircraft_info/aircraft_info_json.c)
_tm_gen_lib(bind           ${_GEN}/bind/bind_json.c)
_tm_gen_lib(device_info
    ${_GEN}/device_info/device_info_publisher.c
    ${_GEN}/device_info/device_info_json.c)

# ============================================================
# App — main + launcher_task (RTOS entry) + all samples
#
#   The RTOS entry point is launcher_task.c. Compiled sources:
#   sample_session.c / sample_device_report.c / bind_monitor.c /
#   sample_aircraft_info.c are OSAL-based and shared by both platforms
#   (sample_session_open is available on RTOS too).
#   sample_launcher.c / sample_registry.c are not compiled here.
# ============================================================

add_executable(payload_iot_demo
    # ── platform entry ──
    ${SAMPLE_DIR}/app/main.c
    ${SAMPLE_DIR}/app/launcher_task.c
    ${SAMPLE_DIR}/app/upgrade_rtos_storage.c
    ${SAMPLE_DIR}/app/upgrade_rtos_http.c
    ${SAMPLE_DIR}/app/upgrade_platform_rtos.c
    ${SAMPLE_DIR}/app/config_rtos.c
    ${SAMPLE_DIR}/app/ca_cert_provider.c
    # ── launcher support (required by sample_session_open, OSAL-based) ──
    ${SDK_ROOT}/samples/launcher/sample_session.c
    ${SDK_ROOT}/samples/launcher/sample_device_report.c
    ${SDK_ROOT}/samples/launcher/bind_monitor.c
    ${SDK_ROOT}/samples/launcher/sample_aircraft_info.c
    ${SDK_ROOT}/samples/launcher/sample_lifecycle.c
    # ── business samples (shared by both platforms, split by #if per platform) ──
    ${SDK_ROOT}/samples/custom_small_data/src/custom_small_data_demo.c
    ${SDK_ROOT}/samples/data_subscription/src/data_subscription_client_demo.c
    ${SDK_ROOT}/samples/power/src/power_client_demo.c
    ${SDK_ROOT}/samples/efts/src/efts_client_demo.c
    ${SDK_ROOT}/samples/time_sync/src/time_sync_client_demo.c
    ${SDK_ROOT}/samples/upgrade/src/upgrade_client_demo.c
    ${SDK_ROOT}/samples/widget/src/widget_client_demo.c
    ${SDK_ROOT}/samples/widget/src/app_widget.c
    ${SDK_ROOT}/samples/widget/src/app_speaker.c
    ${SDK_ROOT}/samples/widget/src/http_file_server.c
    ${SDK_ROOT}/samples/widget/src/widget_md5.c
    # ── cJSON ──
    ${CJSON_SRC}
    # ── startup ──
    ${SAMPLE_DIR}/bsp/drivers/BSP/startup_gd32f5xx_armgcc.s
)
target_include_directories(payload_iot_demo PRIVATE
    ${SAMPLE_DIR}/app
    ${SAMPLE_DIR}/network/tls
    ${SDK_ROOT}/samples/launcher
    ${SDK_ROOT}/samples/widget/src
    ${SDK_ROOT}/samples/upgrade/src
    ${SDK_ROOT}/samples/time_sync/src
    ${SDK_ROOT}/samples/efts/src
    ${SAMPLE_DIR}/bsp/drivers/BSP
    ${SAMPLE_DIR}/bsp/hal
    ${_GEN}
    ${CJSON_INC}
)
target_compile_definitions(payload_iot_demo PRIVATE SAMPLE_DEVICE_NAME="${SAMPLE_DEVICE_SN}")

target_link_libraries(payload_iot_demo PRIVATE
    thing_model_client_runtime
    thing_model_client_transport_lwip
    thing_model_client_crypto_mbedtls
    thing_model_gen_custom_small_data
    thing_model_gen_data_subscription
    thing_model_gen_power
    thing_model_gen_efts
    thing_model_gen_time_sync
    thing_model_gen_upgrade
    thing_model_gen_widget
    thing_model_gen_speaker
    thing_model_gen_aircraft_info
    thing_model_gen_bind
    thing_model_gen_device_info
    thing_model_gen_common
    network
    mbedtls
    lwip
    freertos
    -Wl,--whole-archive bsp -Wl,--no-whole-archive
)

set_target_properties(payload_iot_demo PROPERTIES SUFFIX ".elf")

# ============================================================
# Linker script — gd32f5xx_flash.ld
# ============================================================
set(LINKER_SCRIPT ${SAMPLE_DIR}/ld/gd32f5xx_flash.ld)
target_link_options(payload_iot_demo PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/payload_iot_demo.map
    -Wl,--print-memory-usage
)
set_target_properties(payload_iot_demo PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# ============================================================
# Post-build — generate .bin + .hex + .dis
# ============================================================
add_custom_command(TARGET payload_iot_demo POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:payload_iot_demo>
            ${CMAKE_BINARY_DIR}/payload_iot_demo.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:payload_iot_demo>
            ${CMAKE_BINARY_DIR}/payload_iot_demo.hex
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:payload_iot_demo>
    COMMENT "Generating .bin / .hex / size report"
)
