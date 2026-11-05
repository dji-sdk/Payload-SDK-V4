cmake_minimum_required(VERSION 3.13)
project(thing_model_mqtt_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# -fPIC only on hosted platforms; bare-metal/RTOS (Generic) toolchains reject it.
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_compile_options(-fPIC)
endif()

# ============================================================
# Build configuration axes: platform backend and product profile
#
#   core         product-neutral: session / listener / codec / secure_access
#                + osal|transport|crypto INTERFACES.  Always compiled.
#   platform     THING_MODEL_PLATFORM = host | rtos    (selects backends)
#       host       posix OSAL + mosquitto transport + openssl crypto
#       rtos       freertos OSAL + lwIP transport + mbedTLS crypto
#   profile      THING_MODEL_PROFILE  = uav | dock     (selects product features)
#       uav        payload / generic: NO dock, NO plaintext-access path
#       dock       dock envelope + plaintext-access (defines THING_MODEL_PROFILE_DOCK)
#
# Consumers:
#   • payload (uav)  → -DTHING_MODEL_PLATFORM=host|rtos  -DTHING_MODEL_PROFILE=uav  (default)
#   • edge   (dock)  → -DTHING_MODEL_PLATFORM=host        -DTHING_MODEL_PROFILE=dock
# ============================================================
set(THING_MODEL_PLATFORM "host" CACHE STRING "Target platform: host | rtos")
set_property(CACHE THING_MODEL_PLATFORM PROPERTY STRINGS host rtos)
set(THING_MODEL_PROFILE  "uav"  CACHE STRING "Product profile: uav | dock")
set_property(CACHE THING_MODEL_PROFILE PROPERTY STRINGS uav dock)
message(STATUS "[thing-model] platform=${THING_MODEL_PLATFORM} profile=${THING_MODEL_PROFILE}")

# ============================================================
# Dependency lookup
#   host: OpenSSL / mosquitto — caller prefills paths (bundled third_party) or opts into system lookup.
#   rtos: FreeRTOS / lwIP / mbedTLS include dirs must be prefilled by the sample project.
# ============================================================
if(THING_MODEL_PLATFORM STREQUAL "host")
    option(THING_MODEL_USE_SYSTEM_DEPS "When dependency paths are not prefilled, fall back to looking up system OpenSSL/mosquitto" OFF)

    if(DEFINED OPENSSL_INCLUDE_DIR AND DEFINED OPENSSL_SSL_LIBRARY AND DEFINED OPENSSL_CRYPTO_LIBRARY
       AND DEFINED MOSQUITTO_LIB AND DEFINED MOSQUITTO_INC)
        message(STATUS "[thing-model] Dependency paths already prefilled (third_party self-contained), skipping system lookup")
    elseif(THING_MODEL_USE_SYSTEM_DEPS)
        message(STATUS "[thing-model] THING_MODEL_USE_SYSTEM_DEPS=ON: falling back to system OpenSSL/mosquitto lookup")
        find_package(OpenSSL REQUIRED)
        find_library(MOSQUITTO_LIB mosquitto)
        find_path   (MOSQUITTO_INC mosquitto.h)
        if(NOT MOSQUITTO_LIB OR NOT MOSQUITTO_INC)
            message(FATAL_ERROR
                "libmosquitto not found. Please install it first:\n"
                "  Ubuntu/Debian: sudo apt install libmosquitto-dev\n"
                "  macOS:         brew install mosquitto\n"
                "  RHEL/CentOS:   sudo dnf install mosquitto-devel")
        endif()
    else()
        message(FATAL_ERROR
            "OpenSSL/mosquitto paths not prefilled, and THING_MODEL_USE_SYSTEM_DEPS=OFF.\n"
            "  • Self-contained (recommended): prefill OPENSSL_INCLUDE_DIR/OPENSSL_SSL_LIBRARY/OPENSSL_CRYPTO_LIBRARY\n"
            "                 + MOSQUITTO_LIB/MOSQUITTO_INC pointing to the bundled third_party\n"
            "                 (done automatically by the sample top-level CMakeLists)\n"
            "  • Use system packages: -DTHING_MODEL_USE_SYSTEM_DEPS=ON")
    endif()
    find_package(Threads REQUIRED)

    # openssl link helper (openssl's own imported target vs manual prefill)
    if(TARGET OpenSSL::SSL)
        set(_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
    else()
        set(_OPENSSL_LIBS ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    endif()
else() # rtos
    # lwIP / mbedTLS / FreeRTOS include paths + libs are provided by the sample project (prefilled cache vars).
    foreach(_v FREERTOS_INCLUDE_DIRS LWIP_INCLUDE_DIRS MBEDTLS_INCLUDE_DIRS)
        if(NOT DEFINED ${_v})
            message(FATAL_ERROR "RTOS mode requires the sample project to prefill ${_v} (see the rtos sample CMakeLists).")
        endif()
    endforeach()
endif()

# cJSON: source mode / system package / error. RTOS forces source mode (no system libcjson on embedded).
set(_CJSON_AS_SOURCE FALSE)
if(DEFINED CJSON_SRC)
    if(NOT EXISTS ${CJSON_SRC})
        message(FATAL_ERROR "CJSON_SRC is set but the path does not exist: ${CJSON_SRC}")
    endif()
    if(NOT DEFINED CJSON_INC)
        message(FATAL_ERROR "If CJSON_SRC is set, CJSON_INC (directory containing cJSON.h) must also be set")
    endif()
    set(_CJSON_AS_SOURCE TRUE)
    message(STATUS "[thing-model] cJSON in source mode: ${CJSON_SRC}")
elseif(THING_MODEL_PLATFORM STREQUAL "rtos")
    message(FATAL_ERROR "RTOS mode must prefill CJSON_SRC + CJSON_INC (no system libcjson on embedded)")
else()
    find_library(CJSON_LIB cjson)
    find_path   (CJSON_INC cJSON.h PATH_SUFFIXES cjson)
    if(NOT CJSON_LIB OR NOT CJSON_INC)
        message(FATAL_ERROR
            "libcjson not found. Please install it first, or prefill CJSON_SRC to use source mode:\n"
            "  Ubuntu/Debian: sudo apt install libcjson-dev\n"
            "  macOS:         brew install cjson\n"
            "  Source mode:   set(CJSON_SRC /path/to/cJSON.c CACHE FILEPATH \"\" FORCE)\n"
            "                set(CJSON_INC /path/to         CACHE PATH     \"\" FORCE)")
    endif()
endif()

# ============================================================
# core runtime library —— product-neutral, no direct transport/crypto third-party link
# ============================================================
set(_RUNTIME_SOURCES
    src/thing_model_client_osal.c
    src/thing_model_client_log.c
    src/thing_model_client_codec.c
    src/thing_model_client_secure_access.c
    src/thing_model_client_session.c
)
# OSAL backend: host = posix; rtos = freertos
if(THING_MODEL_PLATFORM STREQUAL "rtos")
    list(APPEND _RUNTIME_SOURCES src/osal/thing_model_client_osal_freertos.c)
else()
    list(APPEND _RUNTIME_SOURCES src/osal/thing_model_client_osal_posix.c)
endif()
# profile=dock adds the dock envelope + enables the plaintext-access path in session.c
if(THING_MODEL_PROFILE STREQUAL "dock")
    list(APPEND _RUNTIME_SOURCES src/thing_model_dock.c)
endif()
if(_CJSON_AS_SOURCE)
    list(APPEND _RUNTIME_SOURCES ${CJSON_SRC})
endif()

add_library(thing_model_client_runtime STATIC ${_RUNTIME_SOURCES})

target_include_directories(thing_model_client_runtime
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src        # log.h lives in src/, samples call LOG_* helpers
            ${CJSON_INC}
)
if(THING_MODEL_PROFILE STREQUAL "dock")
    target_compile_definitions(thing_model_client_runtime PUBLIC THING_MODEL_PROFILE_DOCK)
endif()

if(THING_MODEL_PLATFORM STREQUAL "host")
    target_link_libraries(thing_model_client_runtime PUBLIC Threads::Threads)
else()
    # rtos: only need FreeRTOS headers to compile the OSAL; the sample app does the final link.
    target_include_directories(thing_model_client_runtime PUBLIC ${FREERTOS_INCLUDE_DIRS})
endif()
if(NOT _CJSON_AS_SOURCE)
    target_link_libraries(thing_model_client_runtime PUBLIC ${CJSON_LIB})
endif()

# ============================================================
# platform backends: transport + crypto (one pair per platform)
# ============================================================
if(THING_MODEL_PLATFORM STREQUAL "host")
    # host transport: mosquitto + OpenSSL-SSL (TLS Phase1)
    add_library(thing_model_client_transport_mosquitto STATIC
        src/thing_model_client_transport_mosq.c
    )
    target_include_directories(thing_model_client_transport_mosquitto
        PUBLIC  ${MOSQUITTO_INC}
                ${OPENSSL_INCLUDE_DIR}
    )
    target_link_libraries(thing_model_client_transport_mosquitto
        PUBLIC  thing_model_client_runtime
                ${MOSQUITTO_LIB}
                ${_OPENSSL_LIBS}
    )

    # host crypto: OpenSSL HMAC + RAND
    add_library(thing_model_client_crypto_openssl STATIC
        src/crypto_openssl.c
    )
    target_include_directories(thing_model_client_crypto_openssl
        PUBLIC  ${OPENSSL_INCLUDE_DIR}
    )
    target_link_libraries(thing_model_client_crypto_openssl
        PUBLIC  thing_model_client_runtime
                ${_OPENSSL_LIBS}
    )
else() # rtos
    # rtos transport: lwIP MQTT + altcp_tls (mbedTLS). lwIP/mbedTLS/FreeRTOS are linked by the sample app;
    # here we only provide compile-time include paths so the source compiles.
    add_library(thing_model_client_transport_lwip STATIC
        src/thing_model_client_transport_lwip.c
    )
    target_include_directories(thing_model_client_transport_lwip
        PUBLIC  ${LWIP_INCLUDE_DIRS}
                ${MBEDTLS_INCLUDE_DIRS}
                ${FREERTOS_INCLUDE_DIRS}
    )
    target_link_libraries(thing_model_client_transport_lwip
        PUBLIC  thing_model_client_runtime
    )

    # rtos crypto: mbedTLS HMAC + CTR_DRBG
    add_library(thing_model_client_crypto_mbedtls STATIC
        src/crypto_mbedtls.c
    )
    target_include_directories(thing_model_client_crypto_mbedtls
        PUBLIC  ${MBEDTLS_INCLUDE_DIRS}
    )
    target_link_libraries(thing_model_client_crypto_mbedtls
        PUBLIC  thing_model_client_runtime
    )
endif()
