#=============================================================================
# UT2004 CMake Build System
#
# Replaces the legacy GNU Make build (makefile-header + per-module makefiles).
# Follows the UE1-fgsfdsfgs CMake pattern adapted for UT2004.
#=============================================================================

cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(UT2004 C CXX)

# Emscripten detection (set by emcmake's toolchain file automatically)
if(EMSCRIPTEN)
  message(STATUS "Building for Emscripten/WebAssembly")
endif()

# C++ standard: gnu++14 (CXX_EXTENSIONS ON produces -std=gnu++14)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
  endif()
endif()

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------

option(BUILD_STATIC "Build all modules as static libraries" ON)
# ★ THE THREE WEB BUILD MODES (EMSCRIPTEN only), identical in all four engine repos — ufront 2.19d.
#
#   preset        UT_WEB_PERF  UT_WEB_DEBUG  compile   what it is for
#   wasm-perf     ON           -             -O3 -g0   shipping: fastest, smallest, no symbols
#   wasm          OFF          OFF           -O2 -g    THE DEV DEFAULT: named stacks, DWARF, assertions
#   wasm-debug    OFF          ON            -O0 -g    last resort: every local live, nothing inlined
#
# `wasm` was -O0 here and -O2 in the three UE1 engines, which is backwards from how debugging on this
# project actually goes (engine logs + a symbolised build, not a stepping debugger). -O2 -g gives named
# stacks, readable DWARF and runtime assertions while the engine still RUNS AT A PLAYABLE SPEED, which is
# the point; -O0 is what you switch to when the OPTIMISER is the suspect, because a bug that appears at
# one -O level and not another is almost always undefined behaviour rather than a logic error.
#
# ⚠ THE DEV BUILD IS NOT THE SMALL ONE — measured 2026-08-03, ut2004:
#
#   mode          total     DWARF    code     what dominates
#   wasm-perf     9.4 MB    0.0      8.9      nothing; -g0 strips it all
#   wasm-debug   32.8 MB   21.7      9.8      debug info
#   wasm         49.0 MB   27.4     20.6      debug info AND inlining bloat
#
# -O2's code section is more than TWICE -O0's, because DWARF forces Emscripten to "run only limited
# binaryen optimizations" and these builds link at -O1, so -O2's inlining is never cleaned up the way the
# perf build's is. Speed is what the dev default buys, not size. (Dropping the dev build to -g2 — function
# names in the name section, no DWARF — would reclaim most of the 27 MB and still give named stacks, since
# -O2 cannot honour variable info anyway. Not done; it is a separate change.)
option(UT_WEB_PERF "Emscripten: shipping build — -O3, no assertions, no debug info" OFF)
option(UT_WEB_DEBUG "Emscripten: -O0 instead of the -O2 dev default (nothing inlined, every local live)" OFF)

# Graphics/driver backend selection (mutually-exclusive, HARD-SPLIT driver stacks):
#   legacy = SDL 1.2 (SDLDrv)  + desktop OpenGL (OpenGLDrv)   -> build-legacy/  ut2004-legacy.bin
#   gl4es  = SDL2    (SDL2Drv) + GL4ES          (GL4ESDrv)    -> build-gl4es/    ut2004-gl4es.bin
# The two stacks must NEVER share a binary: SDL 1.2 and SDL 2 export clashing symbols with
# incompatible internal state, and OpenGLDrv (Mesa GL/gl.h) and GL4ESDrv (gl4es GL/gl.h) cannot
# coexist in one translation unit. Each backend builds in its own folder; see CMakePresets.json.
# Emscripten always uses its own forced gl4es-style stack (handled on dedicated branches below).
set(UT_GFX_BACKEND "gl4es" CACHE STRING "Graphics/driver backend: legacy or gl4es")
set_property(CACHE UT_GFX_BACKEND PROPERTY STRINGS legacy gl4es)

if(BUILD_STATIC)
  set(LIB_TYPE STATIC)
  add_definitions(-D__STATIC_LINK=1)
  message(STATUS "Static mode enabled")
else()
  set(LIB_TYPE SHARED)
  add_definitions(-D__STATIC_LINK=0)
endif()

#-----------------------------------------------------------------------------
# Graphics Backend Resolution (back-compat, emscripten force, validate, lock, derive)
#-----------------------------------------------------------------------------

# Back-compat: the old -DUSE_SDL12=ON switch selects the legacy backend.
if(DEFINED USE_SDL12 AND USE_SDL12 AND NOT UT_GFX_BACKEND STREQUAL "legacy")
  message(WARNING "USE_SDL12=ON is deprecated; mapping to UT_GFX_BACKEND=legacy")
  set(UT_GFX_BACKEND "legacy")
endif()

# Emscripten always uses the forced gl4es-style stack; pin the knob so downstream reads agree.
if(EMSCRIPTEN)
  set(UT_GFX_BACKEND "gl4es")
endif()

if(NOT (UT_GFX_BACKEND STREQUAL "legacy" OR UT_GFX_BACKEND STREQUAL "gl4es"))
  message(FATAL_ERROR "UT_GFX_BACKEND must be 'legacy' or 'gl4es' (got '${UT_GFX_BACKEND}')")
endif()

# Lock the backend to this build dir on first configure: the legacy/gl4es stacks cannot be
# swapped inside one build tree (clashing SDL + GL symbols/headers). Force a clean separate dir.
if(DEFINED UT_GFX_BACKEND_LOCKED AND NOT UT_GFX_BACKEND_LOCKED STREQUAL UT_GFX_BACKEND)
  message(FATAL_ERROR
    "This build directory was configured for UT_GFX_BACKEND='${UT_GFX_BACKEND_LOCKED}' but "
    "'${UT_GFX_BACKEND}' was requested.\n"
    "Backends cannot share a build directory. Use a separate folder or the matching preset "
    "(build-legacy / build-gl4es).")
endif()
set(UT_GFX_BACKEND_LOCKED "${UT_GFX_BACKEND}" CACHE INTERNAL "Locked graphics backend for this build dir")

# Derive the legacy switch used throughout the tree (normal var shadows any stale cache entry).
if(UT_GFX_BACKEND STREQUAL "legacy")
  set(USE_SDL12 ON)
else()
  set(USE_SDL12 OFF)
endif()
message(STATUS "Graphics backend: ${UT_GFX_BACKEND} (USE_SDL12=${USE_SDL12})")

#-----------------------------------------------------------------------------
# Architecture Detection
#-----------------------------------------------------------------------------

include(TargetArch)
target_architecture(TARGET_ARCH)

set(TARGET_IS_X86 FALSE)
set(TARGET_IS_64BIT FALSE)

if((TARGET_ARCH MATCHES "86") OR (TARGET_ARCH STREQUAL "ia64"))
  set(TARGET_IS_X86 TRUE)
  if(TARGET_ARCH MATCHES "64")
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "x86_64")
  else()
    set(TARGET_ARCH "x86")
  endif()
endif()

message(STATUS "Target arch: ${TARGET_ARCH}")
message(STATUS "64-bit: ${TARGET_IS_64BIT}")

# Force 32-bit build on x86_64 hosts (per D-05: Linux x86 32-bit only)
# WASM is inherently 32-bit -- -m32 causes emcc to fail.
# Android is a separate 64-bit native build (x86_64 first, arm64 later) -- skip the -m32 demotion.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  if(TARGET_IS_64BIT AND TARGET_IS_X86 AND NOT MSVC)
    message(STATUS "Building x86 binary with x86_64 compiler")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
    # -m32 must also live in CMAKE_<LANG>_FLAGS and the linker flags, not just
    # add_compile_options/add_link_options: try_compile()/check_*() feature
    # probes (notably SDL2's X11 checks) only honor the FLAGS variables. Without
    # this they build 64-bit objects and fail to link against the i386 system
    # libs ("file in wrong format"), silently disabling features such as
    # SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS -- which then makes SDL2 emit
    # a fallback XGenericEventCookie typedef that conflicts with modern Xlib.
    string(APPEND CMAKE_C_FLAGS " -m32")
    string(APPEND CMAKE_CXX_FLAGS " -m32")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -m32")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -m32")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -m32")
  endif()
endif()

#-----------------------------------------------------------------------------
# Platform Defines (per D-06, reproducing makefile-header)
#-----------------------------------------------------------------------------

# Always 32-bit per D-05 -- except the 64-bit Android build (Core.h derives width from __x86_64__/__aarch64__).
if(NOT ANDROID)
  add_definitions(-DPLATFORM_32BIT)
endif()

# Linux platform
# ufront 2.33: these were unconditional, which is fine while every non-web target IS Linux. A Windows
# (MinGW) build must not claim to be Linux — LINUX/__LINUX_X86__ select the POSIX file, socket, threading
# and dynamic-loader paths throughout the engine.
if(NOT WIN32)
  add_definitions(-DLINUX=1)
  add_definitions(-D__LINUX_X86__)
endif()

# Unicode support
# On Emscripten, use ANSI mode (char-based TCHAR) because:
# - wchar_t is 4 bytes on Emscripten (no -fshort-wchar support)
# - UT2004's internal strings are 2-byte encoded
# - 4-byte wchar_t misinterprets 2-byte string data, truncating filenames
# This matches the original Makefile's NO_UNICODE_OS_SUPPORT=1 setting.
if(EMSCRIPTEN OR ANDROID OR MINGW)
  # Android, like Emscripten, uses the ANSI (char-based TCHAR) path. bionic's wchar_t is 32-bit
  # and its <wchar.h> uses a different include guard than glibc's _WCHAR_H, so the engine's 16-bit
  # wcs* redeclarations (UNICODE_BY_HAND block) clash with the system ones pulled in by SDL2.
  # NO_UNICODE_OS_SUPPORT compiles that block out and sidesteps the whole wchar/-fshort-wchar/libc
  # issue -- the same, already-shipping approach as the web build.
  # ufront 2.33 — MinGW joins them for the same reason from the other direction: its wchar_t is 16-bit
  # (the Windows width the engine wants), but it is still a DISTINCT type from the engine's UNICHAR, so
  # every UNICODE_BY_HAND redeclaration of wcslen/wcstoul/_wtof "ambiguates" the one MinGW already
  # declares. Same block, same cure.
  add_definitions(-DNO_UNICODE_OS_SUPPORT=1)
else()
  add_definitions(-DUNICODE=1 -D_UNICODE=1)
endif()

# Case-insensitive file system emulation
add_definitions(-D__FILE_NOCASE=1)

# Karma: disabled by default (per D-09) — the modern web/native stacks don't use it. ufront 2.12 Part B
# restores the BUILD_KARMA_REF cmake path (linking the prebuilt 32-bit metoolkit libs) to exercise it as an
# optional third native binary; the `karma-ref` preset sets it. NO_KARMA stays defined unless it's ON.
option(BUILD_KARMA_REF "Build the 32-bit Karma reference (links the prebuilt metoolkit static libs)" OFF)
if(NOT BUILD_KARMA_REF)
  add_definitions(-DNO_KARMA=1)
else()
  # ufront 2.12 Part B (native): define WITH_KARMA GLOBALLY for ALL modules + add the metoolkit include path
  # globally, exactly as the ORIGINAL v3369 build did. The Epic makefile-header sets
  # `KARMA_CXXFLAGS = -DWITH_KARMA=1 -I$(METOOLKIT_INC)` and applies it to EVERY module's compile — because
  # Engine.h's Karma auto-define excludes __LINUX__, so this global -D is the ONLY source of WITH_KARMA on
  # Linux. Defining it only on the Engine target (the earlier cmake) left the game modules (Onslaught/XGame/
  # Vehicles/...) compiling WITHOUT it, so AActor/ULevel/ASVehicle gained Karma virtuals+members in Engine but
  # not in the game modules -> vtable size/order + sizeof diverged across the module boundary -> spawning a
  # Karma-driven ONS vehicle dispatched a mismatched vtable -> immediate in-game crash (the menu boots fine, it
  # never touches Karma actors). The prebuilt metoolkit libs are byte-identical to the v3369 original drop, so
  # the crash was purely this build inconsistency. (Linked at the Engine target below; the group is on the
  # final exe link line, after the game modules, so their Karma refs resolve.)
  set(METOOLKIT_DIR "${CMAKE_SOURCE_DIR}/Thirdparty/metoolkit" CACHE PATH "metoolkit (Karma SDK) root: include/ + lib.{chk,rel}/")
  set(METOOLKIT_LIB_SUBDIR "lib.rel/linux_single_gcc3.2" CACHE STRING "metoolkit lib variant (rel/chk + toolchain)")
  add_definitions(-DWITH_KARMA=1)
  include_directories(
    ${METOOLKIT_DIR}/include
    ${METOOLKIT_DIR}/include/MdtBcl
    ${METOOLKIT_DIR}/include/MdtKea
    ${METOOLKIT_DIR}/include/McdCommon
    ${METOOLKIT_DIR}/include/McdPrimitives
    ${METOOLKIT_DIR}/include/McdFrame
    ${METOOLKIT_DIR}/include/Mst
    ${METOOLKIT_DIR}/include/MeGlobals
    ${METOOLKIT_DIR}/include/MeApp
  )
endif()

# ufront 2.12 Part B: optional Pixomatic software renderer (the -SOFTWARERENDERER device). The open driver
# source is in-tree (PixoDrv/); the closed Pixomatic core lib is external via PIXOMATIC_DIR (see PixoDrv/
# CMakeLists.txt). Native only; builds in either backend (SDL2/gl4es or SDL 1.2/legacy).
option(USE_PIXOMATIC "Build the Pixomatic software render device (PixoDrv, -SOFTWARERENDERER)" OFF)

# Emscripten: disable x86 SSE intrinsics (WASM has no x86 instruction sets)
if(EMSCRIPTEN)
  add_definitions(-DNO_SSE_SUPPORT=1)
  # ufront 2.13.2 — -pthread must be a COMPILE option, not just a link option.
  #
  # Two reasons, and missing either one is silent:
  #   * it defines __EMSCRIPTEN_PTHREADS__, which is what every threaded code path in this tree is
  #     guarded on (SDL2Viewport's direct WebGL context creation, GL4ESDrv's proc-address resolver,
  #     appRequestExit's MAIN_THREAD_EM_ASM). With it only at link time those guards compile their
  #     SINGLE-THREADED branch, SDL creates the GL context after all, and Emscripten proxies
  #     eglCreateContext to a main thread whose canvas has been transferred away -> InvalidStateError
  #     (Emscripten #24792). The error points at the canvas; the cause is this flag.
  #   * every object in a shared-memory link must carry the atomics/bulk-memory features, and that
  #     includes the FetchContent'd gl4es and the Thirdparty C libraries. Hence it is set HERE, ahead
  #     of FetchContent_MakeAvailable(GL4ES) and every add_subdirectory below, not in SDLLaunch.
  add_compile_options(-pthread)
endif()

# zlib prefix (avoid symbol clashes with system zlib)
add_definitions(-DZ_PREFIX=1)

# Pragma pack support
add_definitions(-DSUPPORTS_PRAGMA_PACK=1)

# Runtime checks (matching makefile-header defaults)
add_definitions(-DDO_CHECK=1 -DDO_GUARD=1 -DDO_CLOCK=1 -DDO_STAT=1)
add_definitions(-DDO_CHECK_SLOW=0 -DDO_GUARD_SLOW=0 -DDO_CLOCK_SLOW=0 -DDO_STAT_SLOW=0)

#-----------------------------------------------------------------------------
# Compiler Flags (per D-06, reproducing makefile-header line 227)
#-----------------------------------------------------------------------------

# Common flags (C and C++)
add_compile_options(
  -pipe
  -fsigned-char
  -fno-strict-aliasing
)
# -fno-lifetime-dse is a GCC-only flag; NDK clang rejects it as an unknown argument (emcc
# silently ignores it, so the Emscripten build is unaffected). Keep it off the Android build.
if(NOT ANDROID)
  add_compile_options(-fno-lifetime-dse)
endif()
# -fshort-wchar makes wchar_t 16-bit (UT2004 expects this for Unicode strings).
# On Emscripten, -fshort-wchar conflicts with libc++ wchar.h which expects 32-bit.
# -fshort-wchar makes wchar_t 16-bit (UT2004 expects this for Unicode strings).
# On Emscripten AND Android, -fshort-wchar conflicts with libc++/bionic wchar.h (32-bit wchar_t);
# both use the NO_UNICODE_OS_SUPPORT (ANSI) path instead, so neither needs -fshort-wchar.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  add_compile_options(-fshort-wchar)
endif()
# -m32 on every native build EXCEPT 64-bit Android.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  add_compile_options(-m32)
endif()

# C++ only flags
if(EMSCRIPTEN)
  # Clang (used by Emscripten) is stricter than GCC about template instantiation
  # and string literal conversions. These flags restore GCC-like leniency:
  # -fdelayed-template-parsing: defer template body parsing like GCC/MSVC
  # -Wno-writable-strings: allow char* from string literals (legacy C++ pattern)
  # -Wno-reserved-user-defined-literal: allow GCC-style string concat in ASM macros
  # -Wno-incompatible-pointer-types: allow legacy UObject* pointer conversions
  string(APPEND CMAKE_CXX_FLAGS " -fdelayed-template-parsing -Wno-writable-strings -Wno-reserved-user-defined-literal -Wno-incompatible-pointer-types")
elseif(ANDROID)
  # Android NDK is clang too -- apply the same GCC-like leniency as the Emscripten path
  # (grow the -Wno-* set as Checkpoint C surfaces new clang diagnostics).
  string(APPEND CMAKE_CXX_FLAGS " -fdelayed-template-parsing -Wno-writable-strings -Wno-reserved-user-defined-literal -Wno-incompatible-pointer-types")
else()
  string(APPEND CMAKE_CXX_FLAGS " -fpermissive -Wno-register")
endif()

#-----------------------------------------------------------------------------
# Build Type Flags
#-----------------------------------------------------------------------------

if(EMSCRIPTEN AND UT_WEB_PERF)
  # Max-performance web build (UT_WEB_PERF=ON): optimize hard, strip debug info, assert-free.
  # -g0 is essential: ANY DWARF (the default RelWithDebInfo build type injects -g) makes Emscripten
  # run only "limited binaryen optimizations", so the wasm stays large. -g0 strips it -> full -O3.
  add_compile_options(-O3 -g0)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
elseif(EMSCRIPTEN AND UT_WEB_DEBUG)
  # Last-resort debug build: nothing inlined, every local live, faithful stepping. Costs ~5x the wasm
  # size of the dev default and a much slower boot, so it is opt-in rather than the default.
  add_compile_options(-O0 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
elseif(EMSCRIPTEN)
  # ★ THE DEV DEFAULT (ufront 2.19d) — was -O0. See the option block at the top for why -O2.
  add_compile_options(-O2 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0 -g)
  add_definitions(-D_DEBUG)
else()
  # Match Phase 1's -O0 debug-oriented build for now
  add_compile_options(-O0 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
endif()

#-----------------------------------------------------------------------------
# x86 Instruction Sets (per makefile-header)
#-----------------------------------------------------------------------------

if(TARGET_IS_X86 AND NOT EMSCRIPTEN AND NOT ANDROID)
  add_compile_options(-march=pentium -mtune=pentiumpro -msse -mmmx)
endif()

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

if(NOT EMSCRIPTEN AND NOT ANDROID)
  add_link_options(-m32)
endif()

#-----------------------------------------------------------------------------
# Dependencies
#-----------------------------------------------------------------------------

# SDL2 via FetchContent (per D-04). Only the gl4es backend uses SDL2; the legacy backend uses the
# vendored SDL 1.2 .so, and Emscripten gets SDL2 from the -sUSE_SDL=2 port.
include(FetchContent)
if(NOT EMSCRIPTEN AND UT_GFX_BACKEND STREQUAL "gl4es")
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.x
    GIT_SHALLOW    TRUE
  )
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
  if(ANDROID)
    # Android: SDL2 SHARED -> libSDL2.so (SDLActivity's Java layer loads it first; the SDL JNI
    # lives in it). SDL2 auto-selects its Android video/audio backend -- no X11/Wayland/i386 hacks.
    set(SDL_SHARED ON  CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    # SDL2Drv/GL4ESDrv/SDLLaunch link SDL2::SDL2-static; that target doesn't exist when SDL is
    # built shared, so alias it to the shared SDL2 -> those refs resolve with zero edits there.
    if(NOT TARGET SDL2::SDL2-static)
      add_library(SDL2::SDL2-static ALIAS SDL2)
    endif()
  else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    if(MINGW)
      # ufront 2.33 (Windows) — SDL2 selects its own Windows video/audio backends. The X11 forcing below
      # is a Linux -m32 cross-compile workaround; applying it here would send SDL2 hunting for X11 libs
      # that do not exist on a Windows target.
      FetchContent_MakeAvailable(SDL2)
    else()
      # Force X11 video driver — SDL2's CMake misses 32-bit X11 libs during -m32 cross-compile.
      # Point find_library and find_path at the i386 multiarch directories.
      set(SDL_X11 ON CACHE BOOL "" FORCE)
      set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
      set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
      list(APPEND CMAKE_LIBRARY_PATH /usr/lib/i386-linux-gnu)
      list(APPEND CMAKE_PREFIX_PATH /usr/lib/i386-linux-gnu)
      set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
      FetchContent_MakeAvailable(SDL2)
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# openal-soft via FetchContent (Android only) -- provides libopenal.so with the OpenSL ES backend so
# the ALAudio subsystem (which dlopens "libopenal.so" at runtime, per ALAudioSubsystem.h AL_DLL) has a
# working OpenAL on Android. Web uses the Emscripten OpenAL port (-lopenal); native desktop uses the
# system libopenal -- neither path reaches this block. Linked into libUT2004.so (SDLLaunch) below so the
# APK packages it per-ABI and it loads at startup; ALAudio's dlopen then returns the already-loaded handle.
#-----------------------------------------------------------------------------
if(ANDROID)
  # openal-soft is modern C++ whose static-init filter tables (core/bsinc_tables.cpp) use a C++17
  # construct (a reference non-type template parameter whose member sizes a std::array). The engine
  # forces CMAKE_CXX_STANDARD 14 (legacy UT2004 tree) + -fdelayed-template-parsing (clang MSVC-compat),
  # both of which openal-soft inherits -> that C++17 code miscompiles to garbage array sizes -> a
  # null-deref (memset) crash in openal's global constructors at dlopen. Build openal-soft as C++17
  # with the compat flag dropped; save/restore so the rest of the engine keeps gnu++14.
  set(_UT_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_UT_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
  string(REGEX REPLACE "-fdelayed-template-parsing" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD 17)
  FetchContent_Declare(
    OpenALSoft
    GIT_REPOSITORY https://github.com/kcat/openal-soft.git
    GIT_TAG        1.23.1
    GIT_SHALLOW    TRUE
  )
  set(LIBTYPE SHARED CACHE STRING "" FORCE)              # -> libopenal.so (not a static archive)
  set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
  set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
  set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
  set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
  set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
  set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
  set(ALSOFT_BACKEND_OPENSL ON CACHE BOOL "" FORCE)       # Android native audio (OpenSL ES)
  set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
  # openal-soft's cmake_minimum_required predates CMake 4.x's floor; force a modern policy (as GL4ES does).
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(OpenALSoft)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  # openal-soft pins CXX_STANDARD 14 on its own targets (DEFAULT_TARGET_PROPS), which overrides the
  # directory-level standard above. Force its targets to C++17 so the bsinc_tables static-init code
  # (compiled into OpenAL via CORE_OBJS) is not miscompiled.
  foreach(_al_tgt OpenAL common)
    if(TARGET ${_al_tgt})
      set_target_properties(${_al_tgt} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    endif()
  endforeach()
  set(CMAKE_CXX_FLAGS "${_UT_SAVED_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD "${_UT_SAVED_CXX_STANDARD}")
endif()

#-----------------------------------------------------------------------------
# GL4ES via FetchContent (per D-03, D-04) -- gl4es backend (and Emscripten) only.
#-----------------------------------------------------------------------------
if(EMSCRIPTEN OR UT_GFX_BACKEND STREQUAL "gl4es")
  # GL4ES source is configurable so a clean checkout builds against our fork (which carries
  # the cubemap/material-combiner fixes) by default, yet can be repointed at stock upstream
  # with two -D flags and nothing else changes. Interface-neutral: if the fork is upstreamed
  # later, just flip these two defaults back to ptitSeb/gl4es + v1.1.6.
  set(GL4ES_GIT_REPOSITORY "https://github.com/10-p/gl4es.git"
      CACHE STRING "GL4ES git remote. Default: 10-p fork (cubemap/material fixes). Upstream: https://github.com/ptitSeb/gl4es.git")
  set(GL4ES_GIT_TAG "fix/cubemap-bind-active-unit"
      CACHE STRING "GL4ES git ref (branch/tag/SHA). Default tracks our fork's integration branch (latest fixes). Upstream: v1.1.6")
  FetchContent_Declare(
    GL4ES
    GIT_REPOSITORY ${GL4ES_GIT_REPOSITORY}
    GIT_TAG        ${GL4ES_GIT_TAG}
    GIT_SHALLOW    TRUE
  )
  # GL4ES build options: static library, GLES2 target, no X11/EGL (SDL2 owns context).
  set(NOX11 ON CACHE BOOL "" FORCE)
  set(NOEGL ON CACHE BOOL "" FORCE)
  set(STATICLIB ON CACHE BOOL "" FORCE)
  set(DEFAULT_ES 2 CACHE STRING "" FORCE)
  set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
  # Ensure GL4ES C sources compile as 32-bit, matching UT2004's -m32 build (Pitfall 7).
  # 64-bit Android does not get -m32.
  if(NOT EMSCRIPTEN AND NOT ANDROID)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32" CACHE STRING "" FORCE)
  endif()
  # CMake-policy floor: stock upstream GL4ES v1.1.6 declares cmake_minimum_required(VERSION
  # 2.8.12), which CMake 4.x rejects ("Compatibility with CMake < 3.5 has been removed"). Force
  # a modern floor so the upstream tag still configures (our fork already declares 2.8.12...3.10,
  # so this is a no-op for the default). GL4ES source overrides:
  #   * stock upstream:  -DGL4ES_GIT_REPOSITORY=https://github.com/ptitSeb/gl4es.git -DGL4ES_GIT_TAG=v1.1.6
  #   * local checkout:  -DFETCHCONTENT_SOURCE_DIR_GL4ES=/abs/path/to/gl4es  (FetchContent honors it automatically)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(GL4ES)
  unset(CMAKE_POLICY_VERSION_MINIMUM)

  # Override GL4ES output directory: GL4ES's CMakeLists.txt sets
  # CMAKE_ARCHIVE_OUTPUT_DIRECTORY to ${CMAKE_SOURCE_DIR}/lib, causing
  # both native and WASM builds to share the same libGL.a file. Redirect
  # each build's GL4ES output to its own binary directory.
  if(TARGET GL)
    set_target_properties(GL PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    # Unambiguous, namespaced handle so GL4ESDrv links the translation layer's libGL.a and can
    # never silently fall back to the system -lGL (the "two versions of OpenGL" hazard).
    if(NOT TARGET gl4es::GL)
      add_library(gl4es::GL ALIAS GL)
    endif()

    # ★ THE WEB DEBUG BUILD MUST NOT COMPILE GL4ES AT -O0 — the module becomes uncompilable.
    #
    # GL4ES asks for -O2 itself (its build type is RelWithDebInfo), but this file's directory-wide
    # `add_compile_options(-O0 -g)` is appended AFTER it and emcc takes the last -O flag, so the
    # non-perf web build was silently handing GL4ES a -O0 it never asked for. That is fatal here,
    # because of what happens downstream:
    #
    #   `generateInstruction` (arbgenerator.c, a ~950-line macro-expanded ARB-assembly switch)
    #     compiled at -O0 declares  4800 locals ->  91016 after -sASYNCIFY instrumentation
    #     compiled at -O2 declares     7 locals ->    358 after -sASYNCIFY instrumentation
    #
    # V8 rejects any function declaring more than 50000 locals, so Chromium refused the WHOLE
    # module: `CompileError: Compiling function #16243:"generateInstruction" failed: local count
    # too large`. Nothing ran — not even static init — which is why this was mis-recorded as a
    # static-init abort. Measured 2026-08-03 by re-running the exact compile command at each level
    # and counting the declared locals in the object, then again through `wasm-opt --asyncify`
    # (binaryen warns about it in as many words: "Some VMs may not accept this binary because it
    # has a large number of locals").
    #
    # -O2, not -O1, because it is what GL4ES's own build type selects — this restores its choice
    # rather than inventing one. `-g` still comes from the directory options, so DWARF for the
    # translation layer is unaffected and its `[GL4ES-DBG]` logging is untouched. The engine's own
    # sources keep -O0; a debug build exists to debug THIS engine, not a vendored dependency.
    #
    # Web only, and only in the -O0 mode: native has neither Asyncify nor V8's limit, and the -O2/-O3
    # dev and perf builds already inline this far past the problem. Narrow rather than
    # `NOT UT_WEB_PERF`, so the condition says exactly what it protects against.
    if(EMSCRIPTEN AND UT_WEB_DEBUG)
      target_compile_options(GL PRIVATE -O2)
      message(STATUS "GL4ES: forcing -O2 in the -O0 web build (at -O0 + Asyncify, "
                     "generateInstruction exceeds V8's 50000-local limit and the module will not compile)")
    endif()
  endif()
endif()

# SDL 1.2 paths are only needed by the legacy backend (vendored 2006 .so + headers).
# USE_SDL12 is derived from UT_GFX_BACKEND above (legacy => ON); it is no longer a separate option.
if(USE_SDL12)
  if(MINGW)
    # ufront 2.33 — no Windows build of SDL 1.2 to vendor; fetch SDL's own prebuilt MinGW binaries.
    include(SDL12Mingw)
  else()
    set(SDL12_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Thirdparty/SDL12/include")
    set(SDL12_LIBRARY "${CMAKE_SOURCE_DIR}/Thirdparty/SDL12/libSDL-1.2.so.0")
  endif()
endif()

# Emscripten: SDL2 is provided by the port flag; add global compile option for SDL.h includes
if(EMSCRIPTEN)
  add_compile_options(-sUSE_SDL=2)
endif()

#-----------------------------------------------------------------------------
# Output Settings
#-----------------------------------------------------------------------------

# Android keeps the default "lib" prefix so libSDL2.so / libUT2004.so are named as SDLActivity
# expects ("lib<name>.so"). Everywhere else the engine clears it exactly as before.
if(NOT ANDROID)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".bin")
endif()

#-----------------------------------------------------------------------------
# Vendored C Libraries
#-----------------------------------------------------------------------------

add_subdirectory(Thirdparty/zlib)
add_subdirectory(Thirdparty/Vorbis/ogg)
add_subdirectory(Thirdparty/Vorbis/vorbis)
add_subdirectory(Thirdparty/Speex)

#-----------------------------------------------------------------------------
# Engine Modules
#-----------------------------------------------------------------------------

add_subdirectory(Source/Core)
add_subdirectory(Source/Engine)
add_subdirectory(Source/Fire)
add_subdirectory(Source/IpDrv)
add_subdirectory(Source/XGame)
add_subdirectory(Source/XInterface)
add_subdirectory(Source/UWeb)
add_subdirectory(Source/Onslaught)
add_subdirectory(Source/ALAudio)

# Viewport + render driver selection -- HARD SPLIT. Each backend builds exactly one viewport
# driver and one render driver; the unused modules are not compiled, linked, or registered.
# The UT_HAS_* vars propagate to SDLLaunch below.
if(EMSCRIPTEN)
  # Emscripten forced stack: SDL2 + GL4ES + WebRTC (matches the shipping web build).
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  add_subdirectory(Source/WebRTCDrv)
  set(UT_HAS_SDL2DRV ON)
  set(UT_HAS_GL4ESDRV ON)
elseif(UT_GFX_BACKEND STREQUAL "legacy")
  # Legacy stack: SDL 1.2 + desktop OpenGL.
  add_subdirectory(Source/SDLDrv)
  add_subdirectory(Source/OpenGLDrv)
  set(UT_HAS_SDLDRV ON)
  set(UT_HAS_OPENGLDRV ON)
else() # gl4es
  # GL4ES stack: SDL2 + GL4ES translation layer.
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  set(UT_HAS_SDL2DRV ON)
  set(UT_HAS_GL4ESDRV ON)
endif()

# ufront 2.12 Part B: optional Pixomatic software render device (-SOFTWARERENDERER). Native only; the
# in-tree driver builds in whichever backend was selected above (SDL2/gl4es or SDL 1.2/legacy).
if(USE_PIXOMATIC AND NOT EMSCRIPTEN)
  add_subdirectory(Source/PixoDrv)
  set(UT_HAS_PIXODRV ON)
endif()

# Editor and UTV2004 excluded per D-10

#-----------------------------------------------------------------------------
# Executable Targets
#-----------------------------------------------------------------------------

add_subdirectory(Source/SDLLaunch)

# ufront 2.12 Part B: ucc-bin — native command-line tool (dedicated server / commandlets). Native only
# (no ucc on the web build; the WASM target has no headless-tool use). Not built for Android either.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  add_subdirectory(Source/UCC)
endif()


# Android JNI process-prep bridge (2.10b): a tiny standalone .so, loaded by GameActivity's static
# block and called before super.onCreate()/SDL_main, that sets UT2004_ANDROID_BASEDIR + chdir's to
# the on-storage System dir so the engine reads the game tree in place. Android NDK target only.
if(ANDROID)
  add_library(ut2004_android_bridge SHARED ${CMAKE_SOURCE_DIR}/Android/app/src/main/cpp/android_bridge.cpp)
  target_link_libraries(ut2004_android_bridge android log)
endif()
