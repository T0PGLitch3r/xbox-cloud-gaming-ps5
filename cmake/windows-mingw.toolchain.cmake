# SPDX-License-Identifier: GPL-3.0-or-later
# Cross build of the Windows desktop app (psbox.exe) from Linux with llvm-mingw
# (https://github.com/mstorsjo/llvm-mingw) or a mingw-w64 GCC on the PATH:
#
#   cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/windows-mingw.toolchain.cmake \
#         -DCMAKE_PREFIX_PATH=<SDL3-devel-mingw>/x86_64-w64-mingw32 \
#         -DXC_VULKAN_HEADERS=<Vulkan-Headers>/include
#   ninja -C build-win psbox
#   tools/desktop/package-windows.sh build-win
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(NOT DEFINED XC_MINGW_PREFIX)
    set(XC_MINGW_PREFIX x86_64-w64-mingw32-)
endif()
find_program(XC_MINGW_CC NAMES ${XC_MINGW_PREFIX}clang ${XC_MINGW_PREFIX}gcc REQUIRED)
find_program(XC_MINGW_CXX NAMES ${XC_MINGW_PREFIX}clang++ ${XC_MINGW_PREFIX}g++ REQUIRED)
find_program(XC_MINGW_RC NAMES ${XC_MINGW_PREFIX}windres REQUIRED)
set(CMAKE_C_COMPILER ${XC_MINGW_CC})
set(CMAKE_CXX_COMPILER ${XC_MINGW_CXX})
set(CMAKE_RC_COMPILER ${XC_MINGW_RC})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
