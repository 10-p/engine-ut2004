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

# MinGW-w64 ships two thread models. `win32` is the default and has no <pthread.h>; `posix` adds
# winpthreads. This engine's platform layer implements its recursive mutexes on pthreads, so it selects
# `posix` and gets them for free rather than having threading code rewritten against CRITICAL_SECTION —
# the sort of change where a subtle mistake is expensive and invisible. winpthreads is linked STATICALLY
# (see CMakeLists.txt), so this costs no extra DLL beside the exe.
set(MINGW_THREAD_MODEL "win32" CACHE STRING "MinGW thread model: win32 (no pthreads) or posix (winpthreads)")
if(MINGW_THREAD_MODEL STREQUAL "posix")
  # Debian/Ubuntu install both models side by side as `-posix`/`-win32`; MSYS2, Arch and Fedora ship a
  # single, already-posix compiler with no suffix.
  find_program(_MINGW_POSIX_GCC ${MINGW_TARGET_ARCH}-w64-mingw32-gcc-posix)
  if(_MINGW_POSIX_GCC)
    set(_MINGW_SUFFIX "-posix")
  else()
    set(_MINGW_SUFFIX "")
  endif()
else()
  set(_MINGW_SUFFIX "")
endif()

set(_MINGW_TRIPLE "${MINGW_TARGET_ARCH}-w64-mingw32")

set(CMAKE_C_COMPILER   ${_MINGW_TRIPLE}-gcc${_MINGW_SUFFIX})
set(CMAKE_CXX_COMPILER ${_MINGW_TRIPLE}-g++${_MINGW_SUFFIX})
set(CMAKE_RC_COMPILER  ${_MINGW_TRIPLE}-windres)

# Look for headers/libs in the cross sysroot, never the host's /usr — a host libSDL2.so or glibc header
# reached by accident produces errors that read like source bugs (glibc's stdio.h vs mingw's io.h
# "conflicting declaration of int mkdir(const char*)" is the giveaway).
set(CMAKE_FIND_ROOT_PATH /usr/${_MINGW_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
