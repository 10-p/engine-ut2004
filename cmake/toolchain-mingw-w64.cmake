# toolchain-mingw-w64.cmake — cross-compile a Windows binary from Linux with MinGW-w64.
#
# The same compiler MSYS2 ships in its `mingw32` environment (`mingw-w64-i686-toolchain`), so a build
# driven from an MSYS2 prompt on Windows needs no toolchain file and lands on the same code paths; this
# file supplies what a cross build cannot infer.
#
# MINGW_TARGET_ARCH: i686 by default (the `windows` preset, with Pixomatic, as retail UT2004), x86_64 for
# the `windows64` preset (which also sets UT_ALLOW_64BIT).
#
# Usage: `cmake --preset windows` / `cmake --preset windows64` (the presets point here and set
# MINGW_THREAD_MODEL=posix). Standalone:
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake -DMINGW_THREAD_MODEL=posix

set(CMAKE_SYSTEM_NAME Windows)
set(MINGW_TARGET_ARCH "i686" CACHE STRING "MinGW-w64 target arch: i686 or x86_64")
set(CMAKE_SYSTEM_PROCESSOR ${MINGW_TARGET_ARCH})

# MinGW-w64 ships two thread models. `win32` is the default and has no <pthread.h>; `posix` adds
# winpthreads. This engine's platform layer implements its recursive mutexes on pthreads, so it needs
# `posix` (the presets set it). winpthreads is linked statically (see CMakeLists.txt), so it costs no
# extra DLL beside the exe.
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
