# ============================================================
# toolchain-aarch64.cmake — CMake toolchain file for aarch64-linux-gnu
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=../../samples/cmake/toolchain-aarch64.cmake \
#         -B build -S .
#
# Or in the sample top-level CMakeLists:
#   set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/../cmake/toolchain-aarch64.cmake)
#
# Defaults to the Ubuntu standard package gcc-aarch64-linux-gnu (sudo apt install gcc-aarch64-linux-gnu).
# Custom compiler: cmake -DTHING_MODEL_AARCH64_COMPILER=/path/to/clang -DTHING_MODEL_AARCH64_SYSROOT=/sysroot ...
# ============================================================

set(CMAKE_SYSTEM_NAME       Linux)
set(CMAKE_SYSTEM_PROCESSOR  aarch64)

# Compiler: users can override via -DTHING_MODEL_AARCH64_COMPILER
if(NOT DEFINED THING_MODEL_AARCH64_COMPILER)
    # Try common versions first (9 / 10 / 11); fall back to the bare name if none found
    foreach(_v IN ITEMS 11 10 9 "")
        if(_v)
            find_program(_cc NAMES aarch64-linux-gnu-gcc-${_v} NO_CACHE)
        else()
            find_program(_cc NAMES aarch64-linux-gnu-gcc NO_CACHE)
        endif()
        if(_cc)
            set(THING_MODEL_AARCH64_COMPILER ${_cc})
            break()
        endif()
    endforeach()
endif()
if(NOT THING_MODEL_AARCH64_COMPILER)
    message(FATAL_ERROR
        "aarch64-linux-gnu-gcc not found. Please install:\n"
        "  Ubuntu/Debian: sudo apt install gcc-aarch64-linux-gnu\n"
        "or specify explicitly: cmake -DTHING_MODEL_AARCH64_COMPILER=/path/to/cross-gcc")
endif()

set(CMAKE_C_COMPILER   ${THING_MODEL_AARCH64_COMPILER})

# C++ compiler (optional; this SDK is C, but the sample may link C++ libraries)
get_filename_component(_cc_dir ${THING_MODEL_AARCH64_COMPILER} DIRECTORY)
get_filename_component(_cc_name ${THING_MODEL_AARCH64_COMPILER} NAME)
string(REPLACE "gcc" "g++" _cxx_name ${_cc_name})
if(EXISTS ${_cc_dir}/${_cxx_name})
    set(CMAKE_CXX_COMPILER ${_cc_dir}/${_cxx_name})
endif()

# Sysroot (optional)
if(DEFINED THING_MODEL_AARCH64_SYSROOT)
    set(CMAKE_SYSROOT ${THING_MODEL_AARCH64_SYSROOT})
    set(CMAKE_FIND_ROOT_PATH ${THING_MODEL_AARCH64_SYSROOT})
endif()

# find_xxx behavior: look for libraries in the target sysroot, and programs (cmake/python/perl etc.) on the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
