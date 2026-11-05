# ============================================================
# toolchain-arm-none-eabi.cmake
# GD32F527 (Cortex-M33 + FPv4-SP) cross-compile toolchain
# ============================================================
#
# Usage:
#   cmake -S samples/sample_things_model_rtos_gd32 \
#         -B build_gd32 \
#         -DCMAKE_TOOLCHAIN_FILE=samples/sample_things_model_rtos_gd32/toolchain-arm-none-eabi.cmake
#
# Note: the toolchain file must be loaded **before project()**; either the top-level CMakeLists passes it via
#      -DCMAKE_TOOLCHAIN_FILE above, or the top-level CMakeLists includes it itself.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
set(CMAKE_SYSTEM_VERSION   1)

# Skip cmake's try_compile link check — the embedded ELF has no _exit/_sbrk fallback,
# so the default link test would fail.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Toolchain executables — found via PATH, or explicitly set ARM_TOOLCHAIN_PREFIX=/path/to/arm-none-eabi-
if(NOT DEFINED ENV{ARM_TOOLCHAIN_PREFIX})
    set(_TC_PREFIX arm-none-eabi-)
else()
    set(_TC_PREFIX $ENV{ARM_TOOLCHAIN_PREFIX})
endif()

set(CMAKE_C_COMPILER   ${_TC_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${_TC_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${_TC_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${_TC_PREFIX}objcopy CACHE FILEPATH "" FORCE)
set(CMAKE_OBJDUMP      ${_TC_PREFIX}objdump CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE         ${_TC_PREFIX}size    CACHE FILEPATH "" FORCE)

# Compile options — strictly consistent with the old project/armgcc/CMakeLists.txt
set(_CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mthumb-interwork")
set(_FPU_FLAGS "-mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(_OPT_FLAGS "-ffunction-sections -fdata-sections -fno-common -fmessage-length=0")

set(CMAKE_C_FLAGS_INIT   "${_CPU_FLAGS} ${_FPU_FLAGS} ${_OPT_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_CPU_FLAGS} ${_FPU_FLAGS} ${_OPT_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${_CPU_FLAGS} ${_FPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${_CPU_FLAGS} ${_FPU_FLAGS} -Wl,-gc-sections -u _printf_float --specs=nano.specs --specs=nosys.specs")

# do not use host paths in find_xxx
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
