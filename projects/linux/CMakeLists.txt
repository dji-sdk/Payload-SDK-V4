# ============================================================
# projects/linux — Things-Model MQTT SDK samples (Linux x86_64 / aarch64)
#
# Two build targets, selected via -DSAMPLE_BUILD (or pass -DCMAKE_TOOLCHAIN_FILE directly):
#
#   host (native x86 build, works once system packages are installed):
#       cmake -DSAMPLE_BUILD=host -B build_host -S .
#       cmake --build build_host -j1
#       Deps: libssl-dev libmosquitto-dev (cJSON uses the vendored source in-tree)
#
#   cross (aarch64 cross-compile):
#       cmake -DSAMPLE_BUILD=cross -B build_aarch64 -S .
#       cmake --build build_aarch64 -j1
#       Requires aarch64-linux-gnu-gcc; target OpenSSL/mosquitto libraries:
#       prebuilt in-tree (if present), otherwise provided via sysroot or
#       -DMOSQUITTO_LIB=... (see "Cross-compile" in the root README).
#
# Directory layout:
#   projects/linux/          <- this file (main.c, config.c, console, list.inc)
#   samples/launcher/        <- shared launcher library (sample_launcher.c, sample_registry.c,
#                              sample_session.c, sample_bind.c, sample_aircraft_info.c,
#                              sample_device_report.c, sample_lifecycle.c, ...)
#   samples/camera/ ...      <- feature samples (platform-neutral)
#   samples/third_party/     <- cJSON / mongoose / prebuilt .so
#   sdk/                     <- SDK runtime + protocol code (bundled snapshot)
# ============================================================

cmake_minimum_required(VERSION 3.13)

# ── Select the build target (must come before project(); toolchain is processed early by cmake) ──
set(SAMPLE_BUILD_DEFAULT host)   # default target for the dev tree is cross
if(NOT DEFINED SAMPLE_BUILD)
    set(SAMPLE_BUILD ${SAMPLE_BUILD_DEFAULT} CACHE STRING "Build target: host | cross")
endif()
set_property(CACHE SAMPLE_BUILD PROPERTY STRINGS host cross)

if(SAMPLE_BUILD STREQUAL "cross" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/cmake/toolchain-aarch64.cmake)
endif()

project(sample_things_model C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SAMPLES_3RD ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/third_party)

set(SAMPLE_DEVICE_FIRMWARE_VERSION "01.00.00.00" CACHE STRING "Device firmware version")

# ── cJSON: both targets use the vendored source in-tree (no system libcjson-dev needed) ──
set(CJSON_SRC ${SAMPLES_3RD}/cJSON/cJSON.c CACHE FILEPATH "" FORCE)
set(CJSON_INC ${SAMPLES_3RD}/cJSON         CACHE PATH     "" FORCE)

# ── OpenSSL / mosquitto: by default everything comes from the in-tree third_party (build touches no system headers/libs) ──
option(SAMPLE_USE_SYSTEM_DEPS "Use system OpenSSL/mosquitto instead of the in-tree third_party" OFF)

if(NOT SAMPLE_USE_SYSTEM_DEPS)
    if(CMAKE_CROSSCOMPILING)
        set(TP_TRIPLET aarch64-linux-gnu)
    else()
        set(TP_TRIPLET x86_64-linux-gnu)
    endif()
    set(TP_ROOT ${SAMPLES_3RD}/${TP_TRIPLET})
    message(STATUS "[sample_things_model] self-contained third_party: ${TP_ROOT}")

    if((NOT EXISTS ${TP_ROOT}/lib/libmosquitto.so) OR (NOT EXISTS ${TP_ROOT}/include/openssl/opensslconf.h))
        message(FATAL_ERROR
            "In-tree third-party deps are incomplete: ${TP_ROOT}\n"
            "   Collect them first: bash ${SAMPLES_3RD}/vendor-deps.sh\n"
            "   Or switch to system packages: -DSAMPLE_USE_SYSTEM_DEPS=ON")
    endif()

    set(OPENSSL_INCLUDE_DIR    ${TP_ROOT}/include             CACHE PATH     "" FORCE)
    set(OPENSSL_SSL_LIBRARY    ${TP_ROOT}/lib/libssl.so       CACHE FILEPATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY ${TP_ROOT}/lib/libcrypto.so    CACHE FILEPATH "" FORCE)
    set(MOSQUITTO_LIB          ${TP_ROOT}/lib/libmosquitto.so CACHE FILEPATH "" FORCE)
    set(MOSQUITTO_INC          ${TP_ROOT}/include             CACHE PATH     "" FORCE)

    add_link_options(-Wl,--disable-new-dtags)
    set(CMAKE_BUILD_RPATH   "$ORIGIN/../lib;${TP_ROOT}/lib")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
else()
    message(STATUS "[sample_things_model] SAMPLE_USE_SYSTEM_DEPS=ON: OpenSSL/mosquitto come from system packages")
endif()

# ============================================================
# Protocol code + runtime source
#   Builds the protocol code from the bundled sdk/ snapshot.
# ============================================================
set(_PSDK_SNAPSHOT ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk)
set(THING_MODEL_PLATFORM host CACHE STRING "" FORCE)   # linux = host platform
if(EXISTS ${_PSDK_SNAPSHOT}/generated)
    set(PAYLOAD_RELEASE_MODE ON)
    message(STATUS "[sample_things_model] building from bundled sdk/ snapshot")
    set(THING_MODEL_PROFILE uav CACHE STRING "" FORCE)
    add_subdirectory(${_PSDK_SNAPSHOT}/runtime/c ${CMAKE_BINARY_DIR}/_thing_model_runtime)
    set(_RUNTIME_INC ${_PSDK_SNAPSHOT}/runtime/c/include)
    set(P _UNUSED_IN_RELEASE)
else()
    set(PAYLOAD_RELEASE_MODE OFF)
    message(STATUS "[sample_things_model] building protocol code")
    message(FATAL_ERROR "This package builds from the bundled sdk/ protocol code; the development toolchain is not included.")
    set(_RUNTIME_INC ${THING_MODEL_DIR}/runtime/c/include)
    set(P ${THING_MODEL_DIR}/proto)
endif()

# ── Common sample scaffolding: session config loading + cooperative cancellation (reused by the launcher and each sample) ──
add_library(sample_common STATIC
    config.c
    ../../samples/launcher/sample_lifecycle.c
    pps_linux_sysfs.c
    efts_pwm_linux_sysfs.c)
target_include_directories(sample_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/time_sync/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/efts/src
    ${_RUNTIME_INC})

# ============================================================
# Single executable: all samples are linked into one binary,
# sample_things_model, with a menu/command dispatcher.
#   • Registry source of truth = sample_list.inc (this directory, committed, hand-edited).
#   • To add/remove a sample: edit one line in sample_list.inc + one sample_add line below + write its .c.
# ============================================================

# ── vendored mongoose (used by widget's HTTP file server) ──
set(MONGOOSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/third_party/mongoose)

# ── Sample registrar: sample_add(<id> SRCS <c...> PROTOS <proto...>) ──
set(SAMPLE_SRCS   "")
set(SAMPLE_PROTOS "")
function(sample_add id)
    cmake_parse_arguments(A "" "" "SRCS;PROTOS" ${ARGN})
    set(SAMPLE_SRCS   ${SAMPLE_SRCS}   ${A_SRCS}   PARENT_SCOPE)
    set(SAMPLE_PROTOS ${SAMPLE_PROTOS} ${A_PROTOS} PARENT_SCOPE)
endfunction()

set(SAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../samples)
# NOTE: P (proto dir) is set above; unused when building from the bundled snapshot.
sample_add(custom_small_data
    SRCS   ${SAMPLES_DIR}/custom_small_data/src/custom_small_data_demo.c
    )
sample_add(data_subscription
    SRCS   ${SAMPLES_DIR}/data_subscription/src/data_subscription_client_demo.c
    )
sample_add(efts
    SRCS   ${SAMPLES_DIR}/efts/src/efts_client_demo.c
    )
sample_add(mop_channel
    SRCS   ${SAMPLES_DIR}/mop_channel/src/mop_channel_demo.c
    )
sample_add(power
    SRCS   ${SAMPLES_DIR}/power/src/power_client_demo.c
    )
sample_add(time_sync
    SRCS   ${SAMPLES_DIR}/time_sync/src/time_sync_client_demo.c
    )
sample_add(upgrade
    SRCS   ${SAMPLES_DIR}/upgrade/src/upgrade_client_demo.c
           ${SAMPLES_DIR}/upgrade/src/upgrade_http_server.c
           ${CMAKE_CURRENT_SOURCE_DIR}/upgrade_platform_posix.c   # upgrade_platform.h Linux impl: real install/reboot/persisted state
           ${CMAKE_CURRENT_SOURCE_DIR}/upgrade_platform_linux.c   # Linux helpers: bin replace / reboot state / network / autorun marker
           ${SAMPLES_DIR}/widget/src/app_widget.c
           ${SAMPLES_DIR}/widget/src/widget_md5.c
           ${MONGOOSE_DIR}/mongoose.c       # shared with widget; REMOVE_DUPLICATES ensures it compiles only once
    )
sample_add(widget
    SRCS   ${SAMPLES_DIR}/widget/src/widget_client_demo.c
           ${SAMPLES_DIR}/widget/src/app_widget.c
           ${SAMPLES_DIR}/widget/src/app_speaker.c
           ${SAMPLES_DIR}/widget/src/http_file_server.c
           ${SAMPLES_DIR}/widget/src/widget_md5.c
           ${MONGOOSE_DIR}/mongoose.c
    )

# ── Common dependency protos: always added to the build list ──

# ── Deduplicate sources: widget and upgrade share mongoose.c ──
list(REMOVE_DUPLICATES SAMPLE_SRCS)

# ── Protocol code: built from the bundled sdk/ snapshot ──
if(PAYLOAD_RELEASE_MODE)
    file(GLOB_RECURSE THING_MODEL_GEN_SOURCES ${_PSDK_SNAPSHOT}/generated/c/*.c)
    set(THING_MODEL_GEN_INCLUDE ${_PSDK_SNAPSHOT}/generated/c)
else()
    list(REMOVE_DUPLICATES SAMPLE_PROTOS)
    thing_model_generate(PROTO_FILES ${SAMPLE_PROTOS} ROLE server)
endif()

set(SAMPLE_LICENSE_PROFILE "" CACHE STRING "License profile (empty = local default)")

# ── Entry-point selection: interactive menu by default. Use autorun for zero-input scenarios. ──
set(SAMPLE_AUTORUN        ""  CACHE STRING "Zero-input auto-run: sample name / all / empty = interactive menu")
set(SAMPLE_AUTORUN_BUDGET "0" CACHE STRING "autorun=all per-sample time limit in seconds, 0 = unlimited")
option(SAMPLE_AUTORUN_LOOP "autorun=all loop the demo indefinitely" OFF)

# Single executable (console is compiled in only for interactive-menu mode)
set(LAUNCHER_SRCS
    main.c
    ../../samples/launcher/sample_launcher.c
    ../../samples/launcher/sample_registry.c
    ../../samples/launcher/sample_session.c          # common session facade
    ../../samples/launcher/sample_supervisor.c       # background service supervisor
    ../../samples/launcher/sample_device_report.c    # common device reporting
    ../../samples/launcher/bind_monitor.c            # common bind-state monitor
    ../../samples/launcher/sample_aircraft_info.c)   # common aircraft-info monitor
if(SAMPLE_AUTORUN STREQUAL "")
    list(APPEND LAUNCHER_SRCS sample_console_posix.c)   # swap for sample_console_uart.c on MCU
endif()

add_executable(sample_things_model
    ${LAUNCHER_SRCS}
    ${SAMPLE_SRCS}
    ${THING_MODEL_GEN_SOURCES})

target_include_directories(sample_things_model PRIVATE
    ${THING_MODEL_GEN_INCLUDE}
    ${_RUNTIME_INC}                                     # runtime headers (needed when building runtime source directly)
    ${CMAKE_CURRENT_SOURCE_DIR}                         # sample_list.inc + sample_config_local.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/launcher  # launcher headers
    ${SAMPLES_DIR}/widget/src                           # app_widget.h / app_speaker.h / ...
    ${SAMPLES_DIR}/upgrade/src                          # upgrade_http_server.h
    ${SAMPLES_DIR}/time_sync/src                        # time_sync_pps.h
    ${SAMPLES_DIR}/efts/src                             # efts_pwm.h
    ${MONGOOSE_DIR}
    ${CJSON_INC})                                       # data_subscription uses cJSON directly

# mongoose serves plaintext HTTP static content only; disable its OpenSSL dependency.
target_compile_definitions(sample_things_model PRIVATE
    MG_ENABLE_OPENSSL=0
    SAMPLE_DEVICE_NAME="rpi_test"
    SAMPLE_DEVICE_FIRMWARE_VERSION="${SAMPLE_DEVICE_FIRMWARE_VERSION}")

if(SAMPLE_LICENSE_PROFILE)
    target_compile_definitions(sample_things_model PRIVATE
        "SAMPLE_LICENSE_PROFILE_${SAMPLE_LICENSE_PROFILE}=1")
    target_compile_definitions(sample_common PRIVATE
        "SAMPLE_LICENSE_PROFILE_${SAMPLE_LICENSE_PROFILE}=1")
endif()

# upgrade PUT firmware upload goes through mongoose: raise the memory buffer limit to 64MB to cover all firmware sizes.
if(NOT DEFINED MG_MAX_RECV_SIZE_BYTES)
    set(MG_MAX_RECV_SIZE_BYTES "(64UL*1024UL*1024UL)")
endif()
target_compile_definitions(sample_things_model PRIVATE
    "MG_MAX_RECV_SIZE=${MG_MAX_RECV_SIZE_BYTES}")

# ── Opt-in REAL firmware install (overwrites the running binary + reboots the host) ──
# OFF by default: the shipped sample is stage-only and never mutates the host. Turning this ON
# compiles the real install/reboot/boot-state code (samples/upgrade/deploy/); it ALSO requires the
# runtime gate PSDK_UPGRADE_APPLY=1 to actually fire. See samples/upgrade/deploy/README.md.
option(UPGRADE_REAL_INSTALL "Compile the opt-in real firmware install + reboot (deploy rigs only)" OFF)
if(UPGRADE_REAL_INSTALL)
    message(STATUS "[sample_things_model] UPGRADE_REAL_INSTALL=ON — real install/reboot compiled in (arm with PSDK_UPGRADE_APPLY=1)")
    target_compile_definitions(sample_things_model PRIVATE UPGRADE_REAL_INSTALL=1)
endif()

# Entry-point macro injection
if(SAMPLE_AUTORUN STREQUAL "all")
    target_compile_definitions(sample_things_model PRIVATE
        SAMPLE_AUTORUN_ALL=1
        SAMPLE_AUTORUN_BUDGET=${SAMPLE_AUTORUN_BUDGET})
    if(SAMPLE_AUTORUN_LOOP)
        target_compile_definitions(sample_things_model PRIVATE SAMPLE_AUTORUN_LOOP=1)
    endif()
elseif(NOT SAMPLE_AUTORUN STREQUAL "")
    target_compile_definitions(sample_things_model PRIVATE SAMPLE_AUTORUN="${SAMPLE_AUTORUN}")
endif()

# Linking the default impls needs --whole-archive (weak symbol factory).
# The runtime/c CMakeLists from the bundled sdk/ snapshot provides these three runtime targets.
target_link_libraries(sample_things_model PRIVATE
    -Wl,--whole-archive
    thing_model_client_transport_mosquitto
    thing_model_client_crypto_openssl
    -Wl,--no-whole-archive
    thing_model_client_runtime
    sample_common
    pthread)

# Bundled runtime libs resolve from ./lib next to the binary (rpath $ORIGIN/lib) — deployment
# needs no LD_LIBRARY_PATH / ldconfig step, which is what makes a launcher script unnecessary.
# BUILD_WITH_INSTALL_RPATH keeps dev-tree absolute paths out of the shipped binary.
set_target_properties(sample_things_model PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN/lib;\$ORIGIN")
if(NOT PAYLOAD_RELEASE_MODE)
    add_dependencies(sample_things_model ${THING_MODEL_CODEGEN_TARGET})
endif()

# Stage the bundled runtime .so next to the binary so rpath $ORIGIN/lib resolves
# them with no system OpenSSL/mosquitto and no LD_LIBRARY_PATH. Without this the
# build-tree binary silently depends on a system libssl.so.1.1 (present on dev
# machines, absent in CI / on target) and dies with "cannot open shared object
# file". Only needed for the in-tree deps case; system deps come from ldconfig.
if(NOT SAMPLE_USE_SYSTEM_DEPS)
    add_custom_command(TARGET sample_things_model POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${TP_ROOT}/lib
                $<TARGET_FILE_DIR:sample_things_model>/lib
        COMMENT "[sample] stage bundled runtime libs into ./lib (rpath \$ORIGIN/lib)")
endif()

# Copy widget config next to the binary; the demo reads from ./config by default.
add_custom_command(TARGET sample_things_model POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SAMPLES_DIR}/widget/config
            $<TARGET_FILE_DIR:sample_things_model>/config
    COMMENT "[sample] copy widget config/ next to sample_things_model")
