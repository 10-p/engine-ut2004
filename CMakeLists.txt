#=============================================================================
# UT2004 CMake build: native Linux, Windows (MinGW), Android and the web (Emscripten).
# Follows the fgsfdsfgs/UE1 CMake pattern; replaces Epic's per-module makefiles.
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
# The three web build modes (Emscripten only), the same in every engine repo:
#
#   preset        UE_WEB_PERF  UE_WEB_DEBUG  compile   what it is for
#   wasm-perf     ON           -             -O3 -g0   shipping: fastest, smallest, no symbols
#   wasm          OFF          OFF           -O2 -g    the dev default and the shipped -debug binary:
#                                                      named stacks, DWARF, assertions, playable speed
#   wasm-debug    OFF          ON            -O0 -g    last resort: every local live, nothing inlined
#
# -O0 is for when the optimiser is the suspect (a bug that appears at one -O level and not another is
# almost always undefined behaviour). The dev build is the largest of the three: DWARF makes Emscripten
# run only limited binaryen optimizations, so -O2's inlining is never cleaned up.
option(UE_WEB_PERF "Emscripten: shipping build — -O3, no assertions, no debug info" OFF)
option(UE_WEB_DEBUG "Emscripten: -O0 instead of the -O2 dev default (nothing inlined, every local live)" OFF)

# One stack everywhere: SDL2 (SDL2Drv) + desktop OpenGL (OpenGLDrv) + GL4ES (GL4ESDrv), all in one binary,
# the render device chosen at startup with -OPENGLRENDERER / -GL4ESRENDERER. OpenGLDrv (Mesa GL/gl.h) and
# GL4ESDrv (gl4es GL/gl.h) set their include directories PRIVATE, so they never share a translation unit,
# and their symbols do not collide: OpenGLDrv links no GL library (it binds through a pointer table filled
# by SDL_GL_GetProcAddress) and gl4es compiles -fvisibility=hidden off-Windows. One device is live at a time.

if(BUILD_STATIC)
  set(LIB_TYPE STATIC)
  add_definitions(-D__STATIC_LINK=1)
  message(STATUS "Static mode enabled")
else()
  set(LIB_TYPE SHARED)
  add_definitions(-D__STATIC_LINK=0)
endif()

# The SDL 1.2 stack does not exist: refuse a request for it by name (an old script or -DUSE_SDL12=ON).
if(DEFINED USE_SDL12 AND USE_SDL12)
  message(FATAL_ERROR
    "USE_SDL12=ON: there is no SDL 1.2 stack. The SDL2 build contains both GL "
    "render devices — run it with -OPENGLRENDERER for the desktop-GL path the legacy binary provided.")
endif()
message(STATUS "Graphics: SDL2; OpenGLDrv + GL4ESDrv in one binary (GL4ESDrv only on the web and Android)")

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

option(UE_ALLOW_64BIT "Build x86_64 instead of demoting to -m32. 32-bit is the default." OFF)
# UE_PTR_BYTES: the pointer width this build will actually produce, computed once so the gates below
# have something trustworthy to test. Neither obvious variable is right on its own:
#   * CMAKE_SIZEOF_VOID_P is fixed during project() and does not see the -m32 demotion applied
#     a few lines below, so it reads 8 for the `native` preset on an x86_64 host.
#   * TARGET_IS_64BIT is derived by string-matching TARGET_ARCH against "86", so an aarch64
#     Android build leaves it FALSE.
# So: start from the compiler's answer and correct it for the three cases that override it.
set(UE_PTR_BYTES ${CMAKE_SIZEOF_VOID_P})
if(EMSCRIPTEN)
  set(UE_PTR_BYTES 4)                       # wasm32; memory64 is explicitly not a target
elseif(ANDROID)
  if(ANDROID_ABI STREQUAL "armeabi-v7a" OR ANDROID_ABI STREQUAL "x86")
    set(UE_PTR_BYTES 4)
  else()
    set(UE_PTR_BYTES 8)                     # arm64-v8a, x86_64
  endif()
elseif(TARGET_IS_X86 AND NOT MSVC AND NOT UE_ALLOW_64BIT)
  # x86 builds as 32-bit unless UE_ALLOW_64BIT says otherwise (the -m32 demotion below), so the answer
  # is 4 whether that demotion is about to happen or already has. Not TARGET_IS_64BIT: CMAKE_C_FLAGS is
  # cached with `-m32`, so on a reconfigure target_architecture() probes an already 32-bit compiler,
  # while CMAKE_SIZEOF_VOID_P keeps the 8 cached on the first configure.
  set(UE_PTR_BYTES 4)
endif()
message(STATUS "Pointer width: ${UE_PTR_BYTES} bytes")

# x86 Linux and Windows build 32-bit by default (-m32); -DUE_ALLOW_64BIT=ON builds x86_64 (the native64
# and windows64 presets). 32-bit is the default because Pixomatic exists only for it (the prebuilts are
# i386-only on both platforms, with no source) and because it is the ktrace control arm. The web is
# wasm32 by nature (-m32 makes emcc fail); Android sets its width per ABI.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  if(TARGET_IS_64BIT AND TARGET_IS_X86 AND NOT MSVC AND NOT UE_ALLOW_64BIT)
    message(STATUS "Building x86 binary with x86_64 compiler")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
    # -m32 must also live in CMAKE_<LANG>_FLAGS and the linker flags, not just
    # add_compile_options/add_link_options: try_compile()/check_*() feature
    # probes (notably SDL2's X11 checks) only honor the FLAGS variables. Without
    # this they build 64-bit objects and fail to link against the i386 system
    # libs ("file in wrong format"), silently disabling features such as
    # SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS, which makes SDL2 emit
    # a fallback XGenericEventCookie typedef that conflicts with modern Xlib.
    string(APPEND CMAKE_C_FLAGS " -m32")
    string(APPEND CMAKE_CXX_FLAGS " -m32")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -m32")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -m32")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -m32")
    # find_library() must look in lib32 as well: without a multiarch triplet (Arch: /usr/lib = 64-bit,
    # /usr/lib32 = i386) SDL2 picks up the 64-bit libX11 and its X11 probes fail exactly as above.
    if(NOT CMAKE_LIBRARY_ARCHITECTURE)
      set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_SUFFIX 32)
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# Platform Defines (reproducing Epic's makefile-header)
#-----------------------------------------------------------------------------

# The pointer width needs no define: Core.h derives PLATFORM_32BITS/PLATFORM_64BITS from the compiler.

# Linux platform
# Not on Windows: LINUX/__LINUX_X86__ select the POSIX file, socket, threading and dynamic-loader paths
# throughout the engine.
if(NOT WIN32)
  add_definitions(-DLINUX=1)
  add_definitions(-D__LINUX_X86__)
endif()

if(MINGW)
  # Link GCC's runtime statically: otherwise the .exe imports libgcc_s_dw2-1.dll and libstdc++-6.dll,
  # which live in the toolchain, not in a Windows install.
  add_link_options(-static-libgcc -static-libstdc++)
  # A posix-thread-model MinGW also pulls in libwinpthread-1.dll through libstdc++ (std::thread in GL4ES);
  # link it statically too, or the exe does not start outside the build machine.
  add_link_options(-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif()

# Unicode support
# Emscripten, Android and MinGW use ANSI mode (char-based TCHAR), as Epic's makefile did with
# NO_UNICODE_OS_SUPPORT=1:
# - wchar_t is 4 bytes on Emscripten (no -fshort-wchar support), and 4-byte wchar_t misinterprets the
#   engine's 2-byte string data
if(EMSCRIPTEN OR ANDROID OR MINGW)
  # bionic's wchar_t is 32-bit and its <wchar.h> uses a different include guard than glibc's _WCHAR_H,
  # so the engine's 16-bit wcs* redeclarations (UNICODE_BY_HAND block) clash with the system ones pulled
  # in by SDL2. MinGW's wchar_t is 16-bit but a distinct type from the engine's UNICHAR, so every
  # UNICODE_BY_HAND redeclaration of wcslen/wcstoul/_wtof is ambiguous with MinGW's own.
  # NO_UNICODE_OS_SUPPORT compiles that block out.
  add_definitions(-DNO_UNICODE_OS_SUPPORT=1)
else()
  add_definitions(-DUNICODE=1 -D_UNICODE=1)
endif()

# Case-insensitive file system emulation
add_definitions(-D__FILE_NOCASE=1)

# USE_KARMA_DECOMP: the recovered Karma, built from source (metoolkit_decomp/src of the fetched
# metoolkit-karma-decomp), the physics on every target and the default. MathEngine shipped Karma as
# binary-only 32-bit x86 archives; the recovered sources are portable C that compile and run at any
# pointer width. Its evidence is that repository's `decomp/proven.txt`.
option(USE_KARMA_DECOMP "Link the RECOVERED Karma (`metoolkit_decomp/src`)" ON)

# BUILD_KARMA_REF: MathEngine's own prebuilt archives, linked as an alternative implementation of the
# same API: the A/B reference arm (the `legacy-karma` preset, and CI), so a shipped build keeps proving
# the recovery against the real thing.
#
# 32-bit x86 Linux only: the archives are binary-only 32-bit x86 objects, there is no 64-bit Linux
# variant, and the `win_amd64_*` variants are MSVC objects the MinGW cross cannot use. Windows is
# refused although it compiles, links and boots: GNU ld drops the cross-object COMDAT relocations of
# `metoolkit/lib.rel/win32_single_msvcrt/*.lib` (e.g. `McdSphylSphereIntersect+0x9d`'s
# `DISP32 ?MeVector3Dot@@YGMQBM0@Z` links as `e8 00 00 00 00`), and each of the 42 such sites in the
# collision core shifts the stack by 4 bytes. There is no ld option for it. The guard below is a
# FATAL_ERROR rather than a silent NO_KARMA, because a "legacy" binary with no legacy Karma in it is
# worse than no binary. BUILD.md has the details.
option(BUILD_KARMA_REF "Link the LEGACY Karma — MathEngine's prebuilt 32-bit LINUX archives (the A/B reference arm)" OFF)
if(USE_KARMA_DECOMP AND BUILD_KARMA_REF)
  message(FATAL_ERROR
    "USE_KARMA_DECOMP and BUILD_KARMA_REF are two implementations of the same API and both "
    "define its symbols. Pick one: BUILD_KARMA_REF for MathEngine's shipped 32-bit archives, "
    "USE_KARMA_DECOMP for the recovered sources.")
endif()
if(BUILD_KARMA_REF)
  # The archives are 32-bit x86 machine code: refuse early and by name rather than let the link fail
  # with a wall of "incompatible" ld notes, or succeed against a stale cache.
  if(EMSCRIPTEN OR ANDROID OR NOT UE_PTR_BYTES EQUAL 4 OR NOT TARGET_ARCH STREQUAL "x86")
    message(FATAL_ERROR
      "BUILD_KARMA_REF is 32-bit x86 ONLY -- MathEngine shipped Karma as binary-only i386 objects "
      "and this build targets ${TARGET_ARCH}${ANDROID_ABI} at ${UE_PTR_BYTES}-byte pointers. "
      "Use USE_KARMA_DECOMP (the recovered sources), which is portable and is the default.")
  endif()
  # Not Windows: the configuration compiles, links and boots, then corrupts its own stack in
  # collision detection (see the option's note above).
  if(WIN32)
    message(FATAL_ERROR
      "BUILD_KARMA_REF is not usable on Windows. MathEngine's win32 archives DO link with MinGW -- "
      "that part of the old 'they are Linux ELF' note was wrong -- but GNU ld drops their "
      "cross-object COMDAT relocations and emits `call` with displacement 0 at 42 sites in the "
      "collision core, each one a silent stack corruption (measured 2026-09-12; see BUILD.md). "
      "Use USE_KARMA_DECOMP on Windows; the legacy A/B arm is Linux-only.")
  endif()
endif()
option(UE_FPMATH_SSE "32-bit x86 only: add -msse2 -mfpmath=sse — the ktrace control arm (the native-sse preset)" OFF)

# Karma at 64-bit: the recovered code carries the LP64 post-passes (metoolkit_decomp/src's README;
# `decomp/test/standalone/lp64_pipeline.sh` in that repository), and LP64 Karma plays on every 64-bit
# target. KD_LP64_OK silences the note below.
if(USE_KARMA_DECOMP AND UE_PTR_BYTES GREATER 4 AND NOT KD_LP64_OK)
  message(STATUS
    "USE_KARMA_DECOMP on a ${UE_PTR_BYTES}-byte-pointer target (arch "
    "${TARGET_ARCH}${ANDROID_ABI}) -- LP64 Karma, which plays: see the note above this line.")
endif()

if(NOT USE_KARMA_DECOMP AND NOT BUILD_KARMA_REF)
  # Say so every time: `option()` reads an existing cache before its default, so a build directory
  # configured with USE_KARMA_DECOMP=OFF keeps NO_KARMA, and the only symptom is a game with no
  # vehicles or ragdolls. `rm -rf build-<preset>` is the fix.
  message(STATUS "Physics: NO_KARMA (no vehicles, no ragdolls) -- "
                 "USE_KARMA_DECOMP=OFF and BUILD_KARMA_REF=OFF. If that is not what you asked "
                 "for, this build directory has a stale cache: delete it and reconfigure.")
  add_definitions(-DNO_KARMA=1)
else()
  # The Karma SDK and the recovered library are fetched, not vendored:
  # https://github.com/10-p/metoolkit-karma-decomp carries the SDK, the recovered sources, the toolchain
  # that produces them and the gates that judge them.
  #
  # Unpinned: GIT_TAG is `main`, because a new version of that repository improves the recovery. For a
  # fixed revision set -DKARMA_DECOMP_GIT_TAG=<sha>, or point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP
  # at a local checkout (also how to build offline, or test an unpushed change to the recovered sources).
  #
  # Declared here, made available much later (just before add_subdirectory): every object in the web's
  # shared-memory link must carry the atomics/bulk-memory features, so the recovered Karma has to be
  # compiled with the global options that follow, -pthread included.
  include(FetchContent)
  # Cache variables, so meta.json records the same repo@ref this configure fetched
  # (cmake/ArtifactDeps.cmake) and a bisect pins the ref with -DKARMA_DECOMP_GIT_TAG=<sha>.
  set(KARMA_DECOMP_GIT_REPOSITORY "https://github.com/10-p/metoolkit-karma-decomp.git"
      CACHE STRING "metoolkit-karma-decomp git remote (the recovered Karma + its SDK)")
  set(KARMA_DECOMP_GIT_TAG "main" CACHE STRING "metoolkit-karma-decomp git ref (a branch: UNPINNED, deliberately)")
  FetchContent_Declare(
    MetoolkitKarmaDecomp
    GIT_REPOSITORY ${KARMA_DECOMP_GIT_REPOSITORY}
    GIT_TAG        ${KARMA_DECOMP_GIT_TAG}
    GIT_SHALLOW    TRUE
  )

  # WITH_KARMA and the metoolkit include path are global, as in the original v3369 build (Epic's
  # makefile-header applies `-DWITH_KARMA=1 -I$(METOOLKIT_INC)` to every module, because Engine.h's Karma
  # auto-define excludes __LINUX__). A module compiled without it disagrees with Engine about
  # AActor/ULevel/ASVehicle's Karma virtuals and members, and spawning a Karma vehicle dispatches a
  # mismatched vtable. METOOLKIT_DIR and the include paths are set with the MakeAvailable, further down.
  add_definitions(-DWITH_KARMA=1)
  # ME_FORCE_CDECL is global for the same reason: it changes the calling convention of every metoolkit
  # declaration. MinGW-w64 predefines WIN32, so MeCall.h would otherwise declare the whole API __stdcall;
  # the recovered sources are cdecl (decompiled from the Linux i386 build). Only for USE_KARMA_DECOMP:
  # MathEngine's own archives are built with the convention MeCall.h declares.
  if(USE_KARMA_DECOMP)
    add_definitions(-DME_FORCE_CDECL=1)
  endif()
endif()

# Optional Pixomatic software renderer (the -SOFTWARERENDERER device, PixoDrv). The closed Pixomatic
# core lives in Thirdparty/Pixomatic (see Source/PixoDrv/CMakeLists.txt). Native 32-bit x86 only.
option(USE_PIXOMATIC "Build the Pixomatic software render device (PixoDrv, -SOFTWARERENDERER)" OFF)

# Pixomatic is a prebuilt i386 archive with no source, so it cannot follow a pointer-width change: at
# 64-bit the link would fail at the very last step. Say so at configure time instead.
if(USE_PIXOMATIC AND UE_PTR_BYTES EQUAL 8 AND NOT ANDROID AND NOT EMSCRIPTEN)
  message(FATAL_ERROR
    "USE_PIXOMATIC=ON is impossible at 64-bit, on EITHER platform, and there is no source to rebuild "
    "from: Thirdparty/Pixomatic/Lib/pixomatic_linux.a is a prebuilt 32-bit i386 archive, and the "
    "Windows side is pixomatic.lib (an import library) against a PE32 i386 pixomatic.dll. Configure "
    "with -DUSE_PIXOMATIC=OFF; 64-bit uses gl4es, which is built from source.")
endif()

# PixoDrv at -O2: utils_surface.h types the RAD surface shim's blit pointer cdecl off Windows (a stdcall
# call site leaves esp 40 bytes low), and UnSkeletalMesh.cpp binds no references to element 0 of empty
# arrays (undefined behaviour the -O2 optimiser exploits). UE_PIXODRV_O0 (Source/PixoDrv/CMakeLists.txt) pins the package
# to -O0 as a diagnostic, off by default; UE_ENGINE_TU_FILES (Source/Engine/CMakeLists.txt) does the same
# per Engine source.

# Emscripten: disable x86 SSE intrinsics (WASM has no x86 instruction sets)
if(EMSCRIPTEN)
  add_definitions(-DNO_SSE_SUPPORT=1)
  # -pthread is a compile option, not just a link option:
  #   * it defines __EMSCRIPTEN_PTHREADS__, which every threaded code path in this tree is guarded on
  #     (SDL2Viewport's direct WebGL context creation, GL4ESDrv's proc-address resolver,
  #     appRequestExit's MAIN_THREAD_EM_ASM); without it those guards compile their single-threaded
  #     branch, and SDL's eglCreateContext is proxied to a main thread whose canvas has been
  #     transferred away (InvalidStateError, Emscripten #24792);
  #   * every object in a shared-memory link must carry the atomics/bulk-memory features, including the
  #     FetchContent'd gl4es and the Thirdparty C libraries, hence here, ahead of
  #     FetchContent_MakeAvailable(GL4ES) and every add_subdirectory below.
  add_compile_options(-pthread)
endif()

# zlib prefix (avoid symbol clashes with system zlib)
add_definitions(-DZ_PREFIX=1)

# Pragma pack support
add_definitions(-DSUPPORTS_PRAGMA_PACK=1)

# Runtime checks (Epic's makefile-header defaults)
add_definitions(-DDO_CHECK=1 -DDO_GUARD=1 -DDO_CLOCK=1 -DDO_STAT=1)
add_definitions(-DDO_CHECK_SLOW=0 -DDO_GUARD_SLOW=0 -DDO_CLOCK_SLOW=0 -DDO_STAT_SLOW=0)

#-----------------------------------------------------------------------------
# Compiler Flags (Epic's makefile-header line 227)
#-----------------------------------------------------------------------------

# Common flags (C and C++)
add_compile_options(
  -pipe
  -fsigned-char
  -fno-strict-aliasing
)
# -fno-lifetime-dse is a GCC-only flag; NDK clang rejects it as an unknown argument (emcc
# ignores it). Not on Android.
if(NOT ANDROID)
  add_compile_options(-fno-lifetime-dse)
endif()
# -fshort-wchar makes wchar_t 16-bit (UT2004 expects this for Unicode strings). Emscripten and Android
# use the NO_UNICODE_OS_SUPPORT (ANSI) path, whose libc++/bionic wchar.h expects 32-bit wchar_t.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  add_compile_options(-fshort-wchar)
endif()
# -m32 on every 32-bit native build. Gated on UE_PTR_BYTES (the width this build actually produces), so
# the status message and the objects cannot disagree.
if(NOT EMSCRIPTEN AND NOT ANDROID AND UE_PTR_BYTES EQUAL 4)
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
  # (grow the -Wno-* set as new clang diagnostics surface).
  string(APPEND CMAKE_CXX_FLAGS " -fdelayed-template-parsing -Wno-writable-strings -Wno-reserved-user-defined-literal -Wno-incompatible-pointer-types")
  # Epic's code calls methods through null pointers on purpose ("Must be safe for NULL objects":
  # UObject::GetFullName's `if( this )`), as MSVC allowed; Clang would delete those checks, and on Android a
  # null page faults.
  string(APPEND CMAKE_CXX_FLAGS " -fno-delete-null-pointer-checks")
else()
  string(APPEND CMAKE_CXX_FLAGS " -fpermissive -Wno-register")
endif()

#-----------------------------------------------------------------------------
# Build Type Flags
#-----------------------------------------------------------------------------
#
# One `-O` per configuration. CMake injects `CMAKE_<LANG>_FLAGS_<CONFIG>` (RelWithDebInfo is
# `-O2 -g -DNDEBUG`) ahead of anything `add_compile_options()` appends, and the compiler takes the last
# `-O`, so the per-config variables are blanked for the configurations handled here and each branch
# states its optimisation level. Blanking also drops their `-DNDEBUG`, so each branch states NDEBUG
# explicitly.
foreach(_cfg DEBUG RELWITHDEBINFO RELEASE)
  set(CMAKE_C_FLAGS_${_cfg} "")
  set(CMAKE_CXX_FLAGS_${_cfg} "")
endforeach()

if(EMSCRIPTEN AND UE_WEB_PERF)
  # Max-performance web build (UE_WEB_PERF=ON): optimize hard, strip debug info, assert-free.
  # -g0 matters: any DWARF (the RelWithDebInfo build type injects -g) makes Emscripten run only
  # "limited binaryen optimizations", so the wasm stays large.
  add_compile_options(-O3 -g0)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
elseif(EMSCRIPTEN AND UE_WEB_DEBUG)
  # Last-resort debug build: nothing inlined, every local live, faithful stepping. Several times the
  # wasm size of the dev default and a much slower boot, so it is opt-in.
  add_compile_options(-O0 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
elseif(EMSCRIPTEN)
  # The dev default, and the shipped `-debug` binary the launcher's "launch debug binary" link loads,
  # so it stays playable as well as symbolised. UE_WEB_DEBUG is the -O0 build.
  add_compile_options(-O2 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0 -g)
  add_definitions(-D_DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3 -g0)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
else()
  # RelWithDebInfo, and an unset build type: what every native/Windows preset asks for.
  add_compile_options(-O2 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
endif()

#-----------------------------------------------------------------------------
# x86 Instruction Sets (Epic's makefile-header)
#-----------------------------------------------------------------------------

if(TARGET_IS_X86 AND NOT EMSCRIPTEN AND NOT ANDROID)
  # `-march=pentium` is an x86-32 CPU and cc1plus refuses it at -m64, so the 64-bit build asks for a
  # 64-bit baseline instead. The 32-bit line is Epic's makefile-header instruction-set line.
  if(UE_ALLOW_64BIT AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_options(-march=x86-64 -mtune=generic -msse2)
  else()
    add_compile_options(-march=pentium -mtune=pentiumpro -msse -mmmx)
    # UE_FPMATH_SSE: the ktrace control arm (`native-sse` preset). A 32-bit build otherwise runs its
    # doubles on x87 at 80-bit and cannot match a 64-bit (SSE) trace.
    if(UE_FPMATH_SSE)
      add_compile_options(-msse2 -mfpmath=sse)
      message(STATUS "UE_FPMATH_SSE: 32-bit float math on SSE2 (the ktrace control arm)")
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

if(NOT EMSCRIPTEN AND NOT ANDROID AND UE_PTR_BYTES EQUAL 4)
  add_link_options(-m32)
endif()

#-----------------------------------------------------------------------------
# Dependencies
#-----------------------------------------------------------------------------

# SDL2 via FetchContent on native, Windows and Android; Emscripten gets SDL2 from its -sUSE_SDL=2 port.
include(FetchContent)

# ---------------------------------------------------------------------------
# ut_provide_samplerate_linker_name: SDL2's `-lsamplerate` on a 64-bit Linux build where only the i386
# dev package is installed. `samplerate.h` is architecture independent, so `libsamplerate0-dev:i386`
# alone satisfies SDL2's `check_include_file(samplerate.h)`; SDL then appends `-lsamplerate` and the
# 64-bit link fails with `cannot find -lsamplerate`, because only the dev package ships the unversioned
# `libsamplerate.so`. The runtime library (`libsamplerate.so.0`) is there, so this links the
# unversioned name to it inside the build tree; if it is missing too, the build names the package to
# install.
function(ut_provide_samplerate_linker_name)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    return()
  endif()
  # Ask the linker itself: `-lsamplerate` is exactly what SDL2 will emit.
  execute_process(COMMAND ${CMAKE_C_COMPILER} -lsamplerate -o /dev/null -xc /dev/null
                  RESULT_VARIABLE _sr_rc OUTPUT_QUIET ERROR_QUIET)
  if(_sr_rc EQUAL 0)
    return()                                    # the dev package is installed
  endif()
  find_library(UE_SAMPLERATE_SONAME NAMES libsamplerate.so.0
               PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib)
  if(NOT UE_SAMPLERATE_SONAME)
    message(STATUS
      "libsamplerate not usable at 64-bit -- disabling SDL2's use of it. "
      "For the better resampler: sudo apt-get install libsamplerate0-dev:amd64")
    set(SDL_LIBSAMPLERATE OFF CACHE BOOL "" FORCE)
    return()
  endif()
  set(_dir "${CMAKE_BINARY_DIR}/complib")
  file(MAKE_DIRECTORY "${_dir}")
  file(CREATE_LINK "${UE_SAMPLERATE_SONAME}" "${_dir}/libsamplerate.so" SYMBOLIC)
  # link_directories() is directory-scoped and SDL2 is added as a subdirectory
  # from HERE, so it reaches SDL's own link; the linker flag covers the targets
  # that record a link line of their own.
  link_directories("${_dir}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${_dir}" PARENT_SCOPE)
  message(STATUS "libsamplerate: no amd64 dev package -- linking through "
                 "${_dir}/libsamplerate.so -> ${UE_SAMPLERATE_SONAME}")
endfunction()

if(NOT EMSCRIPTEN)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.x
    GIT_SHALLOW    TRUE
  )
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  # SDL2main on Windows only: SDL.h renames the app's `main` to `SDL_main`, and SDL2main supplies the
  # WinMain that calls it (without it: "undefined reference to `WinMain@16`"). Elsewhere it is an
  # empty shim.
  if(MINGW)
    set(SDL2_DISABLE_SDL2MAIN OFF CACHE BOOL "" FORCE)
  else()
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
  endif()
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
      # Windows: SDL2 selects its own video/audio backends. The X11 forcing below is the Linux -m32
      # workaround; here it would send SDL2 looking for X11 libs a Windows target does not have.
      FetchContent_MakeAvailable(SDL2)
    elseif(UE_PTR_BYTES EQUAL 4)
      # Force X11 video driver — SDL2's CMake misses 32-bit X11 libs during -m32 cross-compile.
      # Point find_library and find_path at the i386 multiarch directories.
      set(SDL_X11 ON CACHE BOOL "" FORCE)
      set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
      set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
      list(APPEND CMAKE_LIBRARY_PATH /usr/lib/i386-linux-gnu)
      list(APPEND CMAKE_PREFIX_PATH /usr/lib/i386-linux-gnu)
      set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
      FetchContent_MakeAvailable(SDL2)
    else()
      # A native x86_64 build: let SDL2's own find_package find the amd64 X11 dev packages. (Pointing it
      # at /usr/lib/i386-linux-gnu, as the 32-bit branch does, would make every probe fail and
      # SDL_x11xinput2.h emit a fallback XGenericEventCookie that collides with the real one in Xlib.h.)
      set(SDL_X11 ON CACHE BOOL "" FORCE)
      set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
      set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
      ut_provide_samplerate_linker_name()
      FetchContent_MakeAvailable(SDL2)
    endif()
    # SDL takes the platform's wchar_t, not the engine's -fshort-wchar: its HID code hands wchar_t buffers to the C
    # library (mbstowcs, wcsdup: 32-bit on Linux), and a 16-bit one is overrun by every device string it reads. No
    # wchar_t crosses the SDL calls the engine makes.
    target_compile_options(SDL2-static PRIVATE -fno-short-wchar)
  endif()
endif()

#-----------------------------------------------------------------------------
# openal-soft: one fork on every platform that builds it, https://github.com/10-p/openal-soft-web
# `ufront/web` (openal-soft 1.25.2 + the Web Audio backend + the wasm build patches). The web block
# further down builds it static into the wasm; Android and native Linux build it shared and load it at
# audio init. The cache variables are declared once, here, so the fetches cannot drift apart and
# meta.json records what was fetched (cmake/ArtifactDeps.cmake). Windows gets OpenAL Soft's official
# DLL (cmake/OpenALMingw.cmake).
set(OPENALSOFT_GIT_REPOSITORY "https://github.com/10-p/openal-soft-web.git"
    CACHE STRING "openal-soft git remote; its ufront/web branch carries the Web Audio backend")
set(OPENALSOFT_GIT_TAG "ufront/web" CACHE STRING "openal-soft git ref (a branch: a rebuild takes its tip)")

# Native Linux ships its own openal-soft (EAX on; distribution builds may lack EAX): a shared
# libopenal.so that ALAudio dlopens at audio init (never DT_NEEDED: the engine's global operator new is
# exported and GMalloc does not exist yet at process start), copied beside the binary by SDLLaunch's
# POST_BUILD and found through the executable's rpath '.' when run from a System/ directory.
option(UE_BUNDLED_OPENAL "Native Linux: build openal-soft (the ufront fork, EAX on) and put libopenal.so beside the binary, instead of dlopen'ing whatever libopenal the system has" ON)
set(UE_LINUX_OPENALSOFT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID AND NOT EMSCRIPTEN AND UE_BUNDLED_OPENAL)
  set(UE_LINUX_OPENALSOFT ON)
endif()

if(ANDROID OR UE_LINUX_OPENALSOFT)
  # openal-soft is C++20, and the engine's global C++ settings (CMAKE_CXX_STANDARD 14 and the clang
  # -fdelayed-template-parsing, which a FetchContent'd subproject inherits) miscompile its static-init
  # filter tables (core/bsinc_tables.cpp): a null-deref crash in openal's global constructors at dlopen.
  # Build it as C++20 with the compat flag dropped; save/restore so the rest of the engine keeps gnu++14.
  # openal-soft pins CXX_STANDARD 20 on its own targets; the directory-level standard covers its
  # configure-time feature checks.
  set(_UE_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_UE_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
  string(REGEX REPLACE "-fdelayed-template-parsing" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD 20)
  FetchContent_Declare(
    OpenALSoft
    GIT_REPOSITORY ${OPENALSOFT_GIT_REPOSITORY}
    GIT_TAG        ${OPENALSOFT_GIT_TAG}
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
  set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
  if(ANDROID)
    set(ALSOFT_BACKEND_OPENSL ON CACHE BOOL "" FORCE)     # Android native audio (OpenSL ES)
    # No C++20 modules: the NDK's clang 19 rejects libc++'s range adaptors instantiated across a module
    # boundary (al/auxeffectslot.cpp), and below API 30 bionic's C11 threads (tss_set) are header inlines
    # that an importing module does not see. No RTKit: Android has no D-Bus, and its probe would find the
    # build machine's headers.
    set(ALSOFT_ENABLE_MODULES OFF CACHE BOOL "" FORCE)
    set(ALSOFT_RTKIT OFF CACHE BOOL "" FORCE)
  endif()
  # Linux: the backends are whatever the host's headers allow (PulseAudio and ALSA here; PipeWire when its
  # headers are present), each dlopen'd at run time (ALSOFT_DLOPEN), so the .so has no audio DT_NEEDED.
  set(ALSOFT_EAX ON CACHE BOOL "" FORCE)                  # EAX for ALAudio on every platform
  set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
  # openal-soft's cmake_minimum_required predates CMake 4.x's floor; force a modern policy (as GL4ES does).
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(OpenALSoft)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_CXX_FLAGS "${_UE_SAVED_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD "${_UE_SAVED_CXX_STANDARD}")
  if(NOT TARGET OpenAL)
    message(FATAL_ERROR "openal-soft: expected target 'OpenAL' was not created")
  endif()
  message(STATUS "openal-soft (shared, EAX): ${OPENALSOFT_GIT_REPOSITORY}@${OPENALSOFT_GIT_TAG}")
endif()

#-----------------------------------------------------------------------------
# GL4ES via FetchContent (every build: GL4ESDrv is in every binary)
#-----------------------------------------------------------------------------
# The GL4ES source is configurable: a clean checkout builds against the fork (which carries the
# cubemap/material-combiner fixes), and two -D flags repoint it at stock upstream.
set(GL4ES_GIT_REPOSITORY "https://github.com/10-p/gl4es.git"
    CACHE STRING "GL4ES git remote. Default: 10-p fork (cubemap/material fixes). Upstream: https://github.com/ptitSeb/gl4es.git")
set(GL4ES_GIT_TAG "ufront"
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
# GL4ES's C sources compile as 32-bit on the 32-bit native builds, matching the engine's -m32. A
# `CACHE ... FORCE`, so it writes `-m32` into CMAKE_C_FLAGS for the whole project (zlib, ogg, vorbis
# and Speex too); hence only at UE_PTR_BYTES 4.
if(NOT EMSCRIPTEN AND NOT ANDROID AND UE_PTR_BYTES EQUAL 4)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32" CACHE STRING "" FORCE)
endif()
# CMake-policy floor: stock upstream GL4ES v1.1.6 declares cmake_minimum_required(VERSION
# 2.8.12), which CMake 4.x rejects ("Compatibility with CMake < 3.5 has been removed"); the fork
# declares 2.8.12...3.10, so this is a no-op for the default. GL4ES source overrides:
#   * stock upstream:  -DGL4ES_GIT_REPOSITORY=https://github.com/ptitSeb/gl4es.git -DGL4ES_GIT_TAG=v1.1.6
#   * local checkout:  -DFETCHCONTENT_SOURCE_DIR_GL4ES=/abs/path/to/gl4es  (FetchContent honors it automatically)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
FetchContent_MakeAvailable(GL4ES)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# GL4ES's CMakeLists.txt sets CMAKE_ARCHIVE_OUTPUT_DIRECTORY to ${CMAKE_SOURCE_DIR}/lib, which
# would make every build share one libGL.a: redirect each build's output to its own binary directory.
# gl4es names its library target after what it impersonates: `GL`, and `OPENGL32` on Windows (an
# opengl32 replacement). Everything below refers to one handle, so the name is normalised once.
if(TARGET GL)
  set(_UE_GL4ES_TARGET GL)
elseif(TARGET OPENGL32)
  set(_UE_GL4ES_TARGET OPENGL32)
endif()
if(TARGET ${_UE_GL4ES_TARGET})
  set_target_properties(${_UE_GL4ES_TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  # Unambiguous, namespaced handle so GL4ESDrv links the translation layer's libGL.a and can
  # never silently fall back to the system -lGL (the "two versions of OpenGL" hazard).
  if(NOT TARGET gl4es::GL)
    add_library(gl4es::GL ALIAS ${_UE_GL4ES_TARGET})
  endif()

  # The web debug build compiles GL4ES at -O2, not -O0: GL4ES asks for -O2 itself (its build type is
  # RelWithDebInfo), but the directory-wide `add_compile_options(-O0 -g)` comes after it, and at -O0
  # `generateInstruction` (arbgenerator.c, a macro-expanded ARB-assembly switch) declares thousands of
  # locals, tens of thousands after -sASYNCIFY instrumentation. V8 rejects a function with more than
  # 50000 locals, so the whole module would fail to compile ("local count too large"). `-g` still comes
  # from the directory options. Web only, and only in the -O0 mode: native has neither Asyncify nor
  # V8's limit, and the -O2/-O3 builds are far below it.
  if(EMSCRIPTEN AND UE_WEB_DEBUG)
    target_compile_options(${_UE_GL4ES_TARGET} PRIVATE -O2)
    message(STATUS "GL4ES: forcing -O2 in the -O0 web build (at -O0 + Asyncify, "
                   "generateInstruction exceeds V8's 50000-local limit and the module will not compile)")
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
# expects ("lib<name>.so"). Everywhere else the prefix is cleared.
if(NOT ANDROID)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".bin")
endif()

#-----------------------------------------------------------------------------
# Vendored C Libraries
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Karma: populate the fetched SDK + recovered library.
#
# The last thing before add_subdirectory(), so the recovered Karma is compiled with the same global
# options as every other module, -pthread on the threaded web build most importantly (an object
# without the atomics/bulk-memory features is a shared-memory link mismatch that does not announce
# itself).
#-----------------------------------------------------------------------------
# The reference arm populates the same repository and stops there: it wants the SDK's public headers
# and shipped archives, none of the recovered sources, so it never reaches the -O0 pin block below.
if(BUILD_KARMA_REF)
  FetchContent_MakeAvailable(MetoolkitKarmaDecomp)
  # ...and it does not build them either: MakeAvailable adds that directory's targets to `all`, so they
  # are excluded here (the same get_property walk the -O0 pin below uses).
  get_property(_kr_targets DIRECTORY "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit_decomp"
               PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_kr_t ${_kr_targets})
    set_target_properties(${_kr_t} PROPERTIES EXCLUDE_FROM_ALL TRUE)
  endforeach()
  list(LENGTH _kr_targets _kr_n)
  message(STATUS "BUILD_KARMA_REF: ${_kr_n} recovered-Karma target(s) excluded from the build (legacy arm links the shipped archives)")
endif()

if(USE_KARMA_DECOMP)
  FetchContent_MakeAvailable(MetoolkitKarmaDecomp)

  # The recovered Karma C is pinned to -O0 on native/Windows; the engine around it keeps -O2. At -O2,
  # GCC's value-range propagation (-ftree-vrp) folds the exit conditions of the two 3x3 matrix loops in
  # metoolkit_decomp/src/MdtKea/keaCalcIworldandNonInertialForceandVhmf_vanilla.c: the decompiler
  # flattened `m[i][j]` into `m1[0][iVar9 + k]` on a local `MeReal m1[3][3]`, so iterations 2 and 3
  # subscript past the inner array (undefined in C; "-Waggressive-loop-optimizations"), each loop is cut
  # to one iteration and the world inertia tensor is left partly unwritten. That changes the simulation
  # and breaks `decomp/test/ut2004/ktrace_gate.sh`, a byte-for-byte trajectory comparison. The same
  # flattened shape exists in MdtBcl.c and ReadWriteKeaInputToFile.c. clang (the web) does not draw the
  # inference, so the web builds this code at -O3 (wasm-perf) and -O2 (wasm), as validated by its E2E.
  #
  # The code was decompiled from an already-optimised build, so -O0 costs less than it looks.
  #
  # Three knobs, so the pin can be measured:
  #   UE_KARMA_O2_FILES  a regex over decomp source basenames; matching sources get UE_KARMA_O2_OPTIONS
  #                      (a per-source COMPILE_OPTIONS lands after the target's -O0; the last -O wins).
  #                      '.*' is the "everything -O2" arm; a narrower regex is one round of a bisect.
  #                      Empty = the pin as shipped.
  #   UE_KARMA_O2_OPTIONS the options those sources get, default "-O2". A probe is one reconfigure:
  #                      "-O1", "-O2;-fwrapv", "-O2;-ftrivial-auto-var-init=zero", "-O2;-fsanitize=undefined"
  #                      (that one also needs -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined). Scoped to
  #                      the matching decomp sources only, so the engine's own C never changes.
  #   UE_KARMA_PIN_WEB   lift the NOT EMSCRIPTEN guard so the web arm can be built with the pin.
  #                      Default OFF.
  set(UE_KARMA_O2_FILES "" CACHE STRING
      "regex over metoolkit_decomp source basenames to compile at -O2 despite the pin (bisect aid; empty = all pinned)")
  set(UE_KARMA_O2_OPTIONS "-O2" CACHE STRING
      "compile options (;-list) for the sources UE_KARMA_O2_FILES matches (probe aid; default -O2)")
  option(UE_KARMA_PIN_WEB "Also pin the recovered Karma C to -O0 on Emscripten (measurement aid; the shipped wasm is -O3)" OFF)
  if(NOT EMSCRIPTEN OR UE_KARMA_PIN_WEB)
    get_property(_md_targets DIRECTORY "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit_decomp"
                 PROPERTY BUILDSYSTEM_TARGETS)
    set(_md_o2 0)
    foreach(_md_t IN LISTS _md_targets)
      # PRIVATE target options land after the directory-level `add_compile_options(-O2 -g)` above,
      # and the compiler takes the last -O.
      target_compile_options(${_md_t} PRIVATE -O0)
      if(NOT UE_KARMA_O2_FILES STREQUAL "")
        get_target_property(_md_srcs ${_md_t} SOURCES)
        get_target_property(_md_dir ${_md_t} SOURCE_DIR)
        if(_md_srcs)
          foreach(_md_s IN LISTS _md_srcs)
            if(NOT IS_ABSOLUTE "${_md_s}")
              set(_md_s "${_md_dir}/${_md_s}")
            endif()
            get_filename_component(_md_b "${_md_s}" NAME)
            if(_md_b MATCHES "\\.c$" AND _md_b MATCHES "${UE_KARMA_O2_FILES}")
              # Source-file options come after the target's, so this -O2 beats the -O0 above.
              set_source_files_properties("${_md_s}" TARGET_DIRECTORY ${_md_t} PROPERTIES COMPILE_OPTIONS "${UE_KARMA_O2_OPTIONS}")
              math(EXPR _md_o2 "${_md_o2}+1")
            endif()
          endforeach()
        endif()
      endif()
    endforeach()
    list(LENGTH _md_targets _md_n)
    if(EMSCRIPTEN)
      message(STATUS "Karma decomp: ${_md_n} target(s) pinned to -O0 on the web (UE_KARMA_PIN_WEB measurement arm)")
    else()
      message(STATUS "Karma decomp: ${_md_n} target(s) pinned to -O0 (engine keeps -O2)")
    endif()
    if(_md_o2 GREATER 0)
      message(STATUS "Karma decomp: ${_md_o2} source(s) compiled with '${UE_KARMA_O2_OPTIONS}' via UE_KARMA_O2_FILES='${UE_KARMA_O2_FILES}' (probe/bisect arm)")
    endif()
  endif()
endif()

# The SDK's location and its public headers, needed by both Karma arms: the recovered sources
# reimplement what the archives contain and share their headers verbatim.
if(USE_KARMA_DECOMP OR BUILD_KARMA_REF)
  set(METOOLKIT_DIR "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit" CACHE PATH "metoolkit (Karma SDK) root: include/ + lib.{chk,rel}/")
  # Only the reference arm reads this, and that arm is Linux-only (see BUILD_KARMA_REF's note).
  set(METOOLKIT_LIB_SUBDIR "lib.rel/linux_single_gcc3.2" CACHE STRING "metoolkit lib variant (rel/chk + toolchain)")
  # Global, as in the original v3369 build (Epic's makefile-header applies -I$(METOOLKIT_INC) to every
  # module): a module that misses it disagrees about AActor/ULevel/ASVehicle's Karma members.
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

#-----------------------------------------------------------------------------
# openal-soft on the web: replaces Emscripten's built-in OpenAL (-lopenal / libopenal.js), which is
# stereo-only (every source goes through a PannerNode), has no EFX, and proxies every AL call
# synchronously to the browser main thread. openal-soft is the same API, mixed in wasm on its own pthread
# and played by an AudioWorklet (the `web` backend of the ufront/web branch, alc/backends/web.cpp).
#
# Placed here, after the global -pthread compile option and before the modules, for the same reason as
# Karma above. Three things this block gets right:
#  1. The engine's C++ flags are not openal-soft's (see the Android/Linux block above); openal-soft pins
#     its own CXX_STANDARD 20 per target.
#  2. Exceptions. openal-soft reports every AL error by throwing and catching it at the API boundary
#     (context->throw_error), so a link with catching disabled aborts on the first AL error. Wasm-native
#     exceptions cannot be used (Binaryen's Asyncify pass fails the link: "__asyncify_get_call_index does
#     not exist"), so openal-soft alone is compiled with Emscripten's JS exceptions (-fexceptions) and
#     SDLLaunch links with catching enabled; the engine's own objects stay -fignore-exceptions.
#  3. SIMD. Its hand-written SSE mixers compile through Emscripten's translation headers with
#     -msimd128 -msse4.1 (the C mixers are much slower). NEON is off: its header check passes on
#     Emscripten and the runtime would then prefer the NEON path.
#-----------------------------------------------------------------------------
if(EMSCRIPTEN)
  # OPENALSOFT_GIT_REPOSITORY / OPENALSOFT_GIT_TAG: declared once above, shared with Android and Linux.
  FetchContent_Declare(
    OpenALSoft
    GIT_REPOSITORY ${OPENALSOFT_GIT_REPOSITORY}
    GIT_TAG        ${OPENALSOFT_GIT_TAG}
    GIT_SHALLOW    TRUE
  )
  # Directory-scoped: a FetchContent subproject takes CMAKE_<LANG>_FLAGS as they are when it is added,
  # and its feature checks (xmmintrin.h & co.) compile with them too. Restored right after.
  set(_UE_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_UE_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-fdelayed-template-parsing|-Wno-writable-strings|-Wno-reserved-user-defined-literal|-Wno-incompatible-pointer-types"
         "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS " -msimd128 -msse4.1 -fexceptions")
  string(APPEND CMAKE_C_FLAGS " -msimd128 -msse4.1")
  set(LIBTYPE STATIC CACHE STRING "" FORCE)
  foreach(_al_opt ALSOFT_UTILS ALSOFT_EXAMPLES ALSOFT_TESTS ALSOFT_INSTALL ALSOFT_INSTALL_CONFIG
          ALSOFT_INSTALL_HRTF_DATA ALSOFT_INSTALL_AMBDEC_PRESETS ALSOFT_BACKEND_WAVE ALSOFT_CPUEXT_NEON
          ALSOFT_DLOPEN ALSOFT_UPDATE_BUILD_VERSION)
    set(${_al_opt} OFF CACHE BOOL "" FORCE)
  endforeach()
  # EAX 2.0-5.0: ALAudio's EAX path builds on every platform and asks the library at run time.
  # openal-soft's own default is ON only for Windows (a CMake default, not a platform limit).
  set(ALSOFT_EAX ON CACHE BOOL "" FORCE)
  set(ALSOFT_BACKEND_WEB ON CACHE BOOL "" FORCE)
  set(ALSOFT_REQUIRE_SSE4_1 ON CACHE BOOL "" FORCE)  # fail rather than fall back to the slower C mixers
  set(ALSOFT_EMBED_HRTF_DATA ON CACHE BOOL "" FORCE)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(OpenALSoft)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_CXX_FLAGS "${_UE_SAVED_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${_UE_SAVED_C_FLAGS}")
endif()

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

# Viewport and render drivers. The web builds SDL2Drv + GL4ESDrv (+ WebRTCDrv, ProxyNetDrv); the native
# builds SDL2Drv + GL4ESDrv + OpenGLDrv (+ PixoDrv). The UE_HAS_* vars propagate to SDLLaunch below.
if(EMSCRIPTEN)
  # The web stack: SDL2 + GL4ES + WebRTC.
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  add_subdirectory(Source/WebRTCDrv)
  # Proxy networking: a client-only net driver that joins an ordinary native server through a WSS<->UDP
  # game proxy. Experimental, and a package of its own that shares nothing with WebRTCDrv.
  # -DUE_WITH_PROXYNET=OFF removes the package, its JS shim and its registration; P2P is unaffected.
  option(UE_WITH_PROXYNET "Build ProxyNetDrv (join native servers through a game proxy; web only)" ON)
  if(UE_WITH_PROXYNET)
    add_subdirectory(Source/ProxyNetDrv)
  endif()
  set(UE_HAS_SDL2DRV ON)
  set(UE_HAS_GL4ESDRV ON)
else() # gl4es
  # SDL2 + the GL4ES translation layer + desktop OpenGL.
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  set(UE_HAS_SDL2DRV ON)
  set(UE_HAS_GL4ESDRV ON)
  # OpenGLDrv on the same SDL2 stack: one binary carries both GL render devices and the player picks at
  # startup (-OPENGLRENDERER / -GL4ESRENDERER); see the stack note at the top.
  if(NOT EMSCRIPTEN AND NOT ANDROID)
    add_subdirectory(Source/OpenGLDrv)
    set(UE_HAS_OPENGLDRV ON)
  endif()
endif()

# Optional Pixomatic software render device (-SOFTWARERENDERER). Native only.
if(USE_PIXOMATIC AND NOT EMSCRIPTEN)
  add_subdirectory(Source/PixoDrv)
  set(UE_HAS_PIXODRV ON)
endif()

# The Editor and UTV2004 are not built.

#-----------------------------------------------------------------------------
# Executable Targets
#-----------------------------------------------------------------------------

add_subdirectory(Source/SDLLaunch)

# ucc-bin: the native command-line tool (dedicated server / commandlets). Not on the web or Android, and
# not on Windows: UCC.cpp's Windows path wants the UNICODE-era winToUNICODE/winGetSizeUNICODE helpers
# that only UnVcWin32.h provides, and the game does not need it.
if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT MINGW)
  add_subdirectory(Source/UCC)
endif()


# Android JNI process-prep bridge: a small standalone .so, loaded by GameActivity's static block and
# called before super.onCreate()/SDL_main, that sets UT2004_ANDROID_BASEDIR and chdir's to the on-storage
# System dir so the engine reads the game tree in place.
if(ANDROID)
  add_library(ut2004_android_bridge SHARED ${CMAKE_SOURCE_DIR}/Android/app/src/main/cpp/android_bridge.cpp)
  target_link_libraries(ut2004_android_bridge android log)
endif()
