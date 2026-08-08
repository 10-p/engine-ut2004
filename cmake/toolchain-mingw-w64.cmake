# toolchain-mingw-w64.cmake — cross-compile a Windows binary from Linux with MinGW-w64.
#
# ufront step 2.33. This is the SAME compiler MSYS2 ships in its `mingw32` environment
# (`mingw-w64-i686-toolchain`), so a build driven from an MSYS2 prompt on real Windows needs no toolchain
# file at all and lands on the same code paths — this file only supplies what a *cross* build cannot infer.
#
# ⚠ 32-BIT by default, matching this engine's Linux and web builds (CMakeLists.txt demotes an x86_64
# compiler to -m32 and defines PLATFORM_32BIT) and matching retail UT2004 on Windows. Unlike the UE1
# engines this one is NOT inherently 32-bit — the Android port established a working 64-bit path — so
# MINGW_TARGET_ARCH=x86_64 is a reachable follow-up rather than a dead end. It is not the default because
# 64-bit would leave the PLATFORM_32BIT code path that web and Linux exercise every day.
#
# Usage: `cmake --preset windows` (the preset points here). Standalone:
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(MINGW_TARGET_ARCH "i686" CACHE STRING "MinGW-w64 target arch: i686 or x86_64")
set(CMAKE_SYSTEM_PROCESSOR ${MINGW_TARGET_ARCH})

set(_MINGW_TRIPLE "${MINGW_TARGET_ARCH}-w64-mingw32")

set(CMAKE_C_COMPILER   ${_MINGW_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${_MINGW_TRIPLE}-g++)
set(CMAKE_RC_COMPILER  ${_MINGW_TRIPLE}-windres)

# Look for headers/libs in the cross sysroot, never the host's /usr — a host libSDL2.so or glibc header
# reached by accident produces errors that read like source bugs (glibc's stdio.h vs mingw's io.h
# "conflicting declaration of int mkdir(const char*)" is the giveaway).
set(CMAKE_FIND_ROOT_PATH /usr/${_MINGW_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
