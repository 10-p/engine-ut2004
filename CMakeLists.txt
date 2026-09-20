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

# ★ ufront 2.34 — SDL 1.2 IS RETIRED. There is now ONE stack everywhere:
#     SDL2 (SDL2Drv) + desktop OpenGL (OpenGLDrv) + GL4ES (GL4ESDrv), all in one binary,
#     the render device chosen at startup with -OPENGLRENDERER / -GL4ESRENDERER.
#
# What the old hard split claimed, and why neither half holds any more:
#   "SDL 1.2 and SDL 2 export clashing symbols" — true, and moot: SDLDrv and Thirdparty/SDL12 are gone.
#   "OpenGLDrv (Mesa GL/gl.h) and GL4ESDrv (gl4es GL/gl.h) cannot coexist in one translation unit" —
#     literally true, and already contained: both set their include directories PRIVATE, so neither ever
#     sees the other's header. They are never in one TU. Their symbols do not collide either — OpenGLDrv
#     links NO GL library (it binds through a pointer table filled by SDL_GL_GetProcAddress) and gl4es
#     compiles -fvisibility=hidden off-Windows.
# Only ONE device is live at a time: SDL creates one context per window and GRenderSwitches pairs each
# device with its viewport, which is the arrangement the launcher already modelled.
#
# UT_GFX_BACKEND therefore no longer selects "which SDL"; it is kept only so an existing build directory
# and the Android/web branches keep a value to read, and "gl4es" is the only one left.
set(UT_GFX_BACKEND "gl4es" CACHE STRING "Graphics/driver backend (SDL 1.2 retired in 2.34; gl4es only)")
set_property(CACHE UT_GFX_BACKEND PROPERTY STRINGS gl4es)

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

# ufront 2.34 — the SDL 1.2 backend is gone. Fail loudly rather than silently building something else:
# a caller asking for it (an old script, an old preset, -DUSE_SDL12=ON) is asking for a stack that no
# longer exists, and the one that replaced it does everything the legacy one did.
if(DEFINED USE_SDL12 AND USE_SDL12)
  message(FATAL_ERROR
    "USE_SDL12=ON: the SDL 1.2 stack was retired in ufront 2.34. The SDL2 build now contains BOTH GL "
    "render devices — run it with -OPENGLRENDERER for the desktop-GL path the legacy binary provided.")
endif()
if(NOT UT_GFX_BACKEND STREQUAL "gl4es")
  message(FATAL_ERROR
    "UT_GFX_BACKEND='${UT_GFX_BACKEND}': only 'gl4es' remains (SDL 1.2 retired in ufront 2.34). "
    "That stack carries OpenGLDrv as well; select it at runtime with -OPENGLRENDERER.")
endif()

# Emscripten always uses the forced gl4es-style stack; pin the knob so downstream reads agree.
if(EMSCRIPTEN)
  set(UT_GFX_BACKEND "gl4es")
endif()

set(USE_SDL12 OFF)
message(STATUS "Graphics backend: ${UT_GFX_BACKEND} (SDL2; OpenGLDrv + GL4ESDrv in one binary)")

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

option(UT_ALLOW_64BIT "Build x86_64 instead of demoting to -m32. A supported configuration (ufront 2.54); 32-bit remains the default." OFF)
# UT_PTR_BYTES — the pointer width this build will ACTUALLY produce, computed once so the
# Karma gate below has something trustworthy to test.
#
# Neither of the obvious variables is correct on its own:
#   * CMAKE_SIZEOF_VOID_P is fixed during project() and does NOT see the -m32 demotion applied
#     a few lines below, so it reads 8 for the `native` preset on an x86_64 host.
#   * TARGET_IS_64BIT is derived by string-matching TARGET_ARCH against "86", so an aarch64
#     Android build leaves it FALSE — the one case where getting it wrong ships broken physics.
# So: start from the compiler's answer and correct it for the three cases that override it.
set(UT_PTR_BYTES ${CMAKE_SIZEOF_VOID_P})
if(EMSCRIPTEN)
  set(UT_PTR_BYTES 4)                       # wasm32; memory64 is explicitly not a target
elseif(ANDROID)
  if(ANDROID_ABI STREQUAL "armeabi-v7a" OR ANDROID_ABI STREQUAL "x86")
    set(UT_PTR_BYTES 4)
  else()
    set(UT_PTR_BYTES 8)                     # arm64-v8a, x86_64
  endif()
elseif(TARGET_IS_X86 AND NOT MSVC AND NOT UT_ALLOW_64BIT)
  # This tree builds x86 as 32-bit unless UT_ALLOW_64BIT says otherwise (D-05, and the -m32
  # demotion below), so the answer is 4 whether that demotion is about to happen or already has.
  # ⚠ AND IT HAS TO HONOUR THAT OPTION OR IT LIES BY 4. Without this the 64-bit vehicle
  # configured as "Pointer width: 4 bytes" and every gate keyed on UT_PTR_BYTES — including the
  # Karma LP64 note — read the wrong width on the one build that exists to test that width.
  #
  # ⚠ Testing TARGET_IS_64BIT here instead is WRONG ON A RECONFIGURE, and it fails the safe way
  # round only by luck. CMAKE_C_FLAGS is cached WITH `-m32`, so on the second configure
  # target_architecture() probes a compiler that is already 32-bit and reports `x86` — making
  # TARGET_IS_64BIT false and the demotion look unnecessary — while CMAKE_SIZEOF_VOID_P keeps
  # the 8 it cached on the FIRST configure, before -m32 existed. Fresh dir said 4, stale dir
  # said 8, same preset. Measured, not reasoned about.
  set(UT_PTR_BYTES 4)
endif()
message(STATUS "Pointer width: ${UT_PTR_BYTES} bytes")

# Force 32-bit build on x86_64 hosts (per D-05: Linux x86 32-bit only)
# WASM is inherently 32-bit -- -m32 causes emcc to fail.
# Android is a separate 64-bit native build (x86_64 first, arm64 later) -- skip the -m32 demotion.
#
# ★★ ufront 2.54 — -DUT_ALLOW_64BIT=ON IS NOW A SUPPORTED CONFIGURATION, NOT A DEBUGGING VEHICLE.
# The owner's call: both widths are shippable and CI builds both for Linux and Windows. 32-bit stays
# the DEFAULT — it is what the test suite is calibrated on, and it is the only width Pixomatic exists
# for, so demoting by default keeps `native` and the ktrace 32-bit control arm intact.
#
# ⚠ WHAT 64-BIT COSTS YOU, so it is chosen rather than stumbled into: no Pixomatic (see the guard
# further down — the prebuilts are i386-only on BOTH platforms and there is no source), and the
# pointer-width diagnostics recorded below are real work that compiling cleanly does not discharge.
#
# The original note, kept because the measurement still stands:
# The Android x86_64 build ALREADY runs this engine at 64-bit — with Karma off it
# reaches UT2k4MainMenu.Opened() off the retail packages — so "the engine cannot
# do LP64" is refuted, and the only thing that crashes there is Karma. What is
# missing is a way to DEBUG that: on Android the engine installs its own SIGSEGV
# handler, so there is no tombstone and no backtrace, and there is no gdb, no
# ASan and no valgrind. An x86-64 LINUX build is the same pointer width with all
# of those.
#
# ⚠ D-05 RELAXED AT 2.54: 32-bit is still the DEFAULT, but no longer the only shippable width.
# Measured 2026-08-30 before this option existed: all 200 engine translation
# units compile at -m64 with ZERO hard errors and 198 pointer-width diagnostics
# (`-Wint-to-pointer-cast` / `-Wpointer-to-int-cast`), 124 of them in Engine —
# the same class the Karma recovery took from 7,714 to 134. Compiling is not
# porting; this option is what lets the rest of that be measured rather than
# guessed.
if(NOT EMSCRIPTEN AND NOT ANDROID)
  if(TARGET_IS_64BIT AND TARGET_IS_X86 AND NOT MSVC AND NOT UT_ALLOW_64BIT)
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

if(MINGW)
  # ufront 2.33 — link GCC's runtime statically. Without this the .exe imports libgcc_s_dw2-1.dll and
  # libstdc++-6.dll, which live in the toolchain (or MSYS2's /mingw32/bin), NOT in a Windows install: the
  # binary runs on the build machine and dies with "not found" everywhere else. SDL.dll still ships beside
  # it (SDL 1.2 gives us an import library, not a static one).
  add_link_options(-static-libgcc -static-libstdc++)
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

# USE_KARMA_DECOMP — the RECOVERED Karma, built from source in `metoolkit_decomp/src`/.
# ON BY DEFAULT since 2026-08-27: this is now the physics for every target that can take it.
#
# This is the point of the whole metoolkit-karma-decomp effort: MathEngine shipped Karma as binary-only
# 32-bit x86 archives, so linking THOSE can never work off x86 and the web and Android
# builds were therefore always NO_KARMA — no vehicles, no ragdolls. The recovered sources are
# ordinary portable C, so they compile and RUN wherever the pointer width is right.
#
# EVIDENCE FOR THE DEFAULT, so it is not taken on trust: the i386 drop-in plays a full match
# against a metoolkit with every shipped member deleted, and the wasm build passes the whole
# ut2004 browser suite (51/51) equal to a NO_KARMA control built from the same tree.
# metoolkit-karma-decomp `decomp/proven.txt`, entries WASM-SIGMISMATCH and WASM-INDIRECT-SIGS.
option(USE_KARMA_DECOMP "Link the RECOVERED Karma (`metoolkit_decomp/src`)" ON)

# BUILD_KARMA_REF — the LEGACY Karma: MathEngine's own prebuilt archives, linked as an alternative
# implementation of the same API. It is the A/B REFERENCE ARM, and as of 2026-09-12 it is the only
# one: `native-nokarma` (physics vs. no physics) has been retired in CI in its favour, because
# comparing the recovery against the REAL THING answers "is the recovered code right" and comparing
# it against nothing only ever answered "is Karma involved at all".
#
# ⚠ IT WAS REMOVED ON 2026-09-02 AND IS BACK ON 2026-09-12, so read the removal commit (c591d23)
# before deciding it is dead weight again: that commit's argument was that a second implementation
# bought nothing once the recovery was proven, and the owner's answer is that A/B against the
# shipped archives is exactly what a shipped BUILD should keep proving, per push, on both platforms
# people actually download.
#
# ⚠ 32-BIT x86 ONLY, and that is the archives' constraint, not a policy: MathEngine shipped Karma
# as binary-only 32-bit x86 objects. There is no 64-bit Linux variant at all, and the `win_amd64_*`
# variants are MSVC/Whidbey objects the MinGW cross cannot use. The guard below is a FATAL_ERROR
# rather than a silent NO_KARMA because a "legacy" binary with no legacy Karma in it is worse than
# no binary.
#
# ⚠⚠ AND IT IS LINUX-ONLY, WHICH WAS ESTABLISHED BY BUILDING THE WINDOWS ONE AND MEASURING IT
# (2026-09-12). The removal commit's reason — "MathEngine's Windows archives are 32-bit Linux ELF"
# — is FALSE: `metoolkit/lib.rel/win32_single_msvcrt/*.lib` are real MS COFF archives whose Karma
# entry points are plain `__stdcall`-decorated C (`_McdNullCreate@4`). MinGW compiles, links and
# BOOTS them; the exe loads a map, starts a match and simulates Karma. It then dies about twelve
# frames in, and the reason is not fixable from here:
#
#   ★ GNU ld SILENTLY DROPS CROSS-OBJECT COMDAT RELOCATIONS IN THESE ARCHIVES. In
#     `McdSphylSphereIntersect+0x9d` the object has `DISP32 ?MeVector3Dot@@YGMQBM0@Z`; the linked
#     image has `e8 00 00 00 00` — displacement ZERO, so the `call` targets the NEXT INSTRUCTION.
#     That pushes a return address nothing ever pops, ESP stays 4 low for the rest of the function,
#     and every `N(%esp)` after it reads the wrong slot. The crash is a geometry pointer read out
#     of a stack slot holding a float, exactly as that predicts, and the bogus return address
#     0x00a84662 is visible in the crash dump's stack scan.
#     ⚠ NOT A FLAG PROBLEM: 42 such sites across McdBoxBoxIntersect, McdGjkTest and the
#     triangle-list intersectors — the core of collision detection — and the count and the bytes
#     are IDENTICAL with `--exclude-libs` and `--export-all-symbols` both removed. ld resolves a
#     COMDAT kept from the SAME object and fails across objects; there is no ld option for it.
#     Each site is a silent stack corruption, so "it mostly works" is the dangerous reading.
#
# The `windows-legacykarma` preset, the MSVC compiler-support shim (KarmaMsvcCompat.c, six
# routines, its stack probe unit-tested correct) and the CI arm all existed and were REMOVED on
# 2026-09-12 rather than shipped. Git history has them if binutils ever fixes COMDAT handling.
# BUILD.md has the full account. Do not re-derive this: it is a linker defect, not a build recipe.
option(BUILD_KARMA_REF "Link the LEGACY Karma — MathEngine's prebuilt 32-bit LINUX archives (the A/B reference arm)" OFF)
if(USE_KARMA_DECOMP AND BUILD_KARMA_REF)
  message(FATAL_ERROR
    "USE_KARMA_DECOMP and BUILD_KARMA_REF are two implementations of the same API and both "
    "define its symbols. Pick one: BUILD_KARMA_REF for MathEngine's shipped 32-bit archives, "
    "USE_KARMA_DECOMP for the recovered sources.")
endif()
if(BUILD_KARMA_REF)
  # The archives are 32-bit x86 machine code. Nothing about that is fixable from here, so refuse
  # early and by name rather than let the link fail 20 minutes in with a wall of "incompatible"
  # ld notes — or, worse, succeed against a stale cache.
  if(EMSCRIPTEN OR ANDROID OR NOT UT_PTR_BYTES EQUAL 4 OR NOT TARGET_ARCH STREQUAL "x86")
    message(FATAL_ERROR
      "BUILD_KARMA_REF is 32-bit x86 ONLY -- MathEngine shipped Karma as binary-only i386 objects "
      "and this build targets ${TARGET_ARCH}${ANDROID_ABI} at ${UT_PTR_BYTES}-byte pointers. "
      "Use USE_KARMA_DECOMP (the recovered sources), which is portable and is the default.")
  endif()
  # ⚠ AND NOT WINDOWS. This refuses a configuration that COMPILES, LINKS AND BOOTS, so the guard
  # has to exist: without it the next person gets a plausible-looking binary that corrupts its own
  # stack in collision detection. See the option's note above for the measurement.
  if(WIN32)
    message(FATAL_ERROR
      "BUILD_KARMA_REF is not usable on Windows. MathEngine's win32 archives DO link with MinGW -- "
      "that part of the old 'they are Linux ELF' note was wrong -- but GNU ld drops their "
      "cross-object COMDAT relocations and emits `call` with displacement 0 at 42 sites in the "
      "collision core, each one a silent stack corruption (measured 2026-09-12; see BUILD.md). "
      "Use USE_KARMA_DECOMP on Windows; the legacy A/B arm is Linux-only.")
  endif()
endif()
option(UT_FPMATH_SSE "32-bit x86 only: add -msse2 -mfpmath=sse — the ktrace CONTROL arm (see BUILD.md, ufront 2.58)" OFF)

# ★ THE POINTER-WIDTH GATE, LIFTED 2026-08-29. The recovered code used to encode i386 STRUCT
# LAYOUTS — not merely 32-bit pointers but the sizes and byte offsets those pointers produce. At
# LP64, 128 of 151 structs change size (`sizeof(_McdGeometry)` is 16 on i386 and 32 on arm64), so
# a recovered expression that reads `trilistgeom[3].mRefCtAndID` addressed byte 96 where the
# original meant byte 48. NOTHING WAS TRUNCATED, the cast was width-correct, and clang was silent.
#
# ``metoolkit_decomp/src`/` now carries the LP64 post-passes (see its README), and
# `metoolkit-karma-decomp `decomp/test/standalone/lp64_pipeline.sh`` ends `-> PASS`.
#
# ⚠ THAT SENTENCE USED TO REST ON A FLOOR, AND THE FLOOR WAS MEASURING ARITHMETIC. It read
# "every trajectory first differs at the float floor — 9.0e-10, 1.9e-09 and 2.2e-07" while the
# 64-bit ragdoll was giving a DIFFERENT ANSWER ON EVERY RUN (proven.txt LP64-ADDRESS-DEPENDENT),
# because -mfpmath=387 equalises the format and not the excess precision. Since 2026-08-30 the
# gate is a bit comparison with the arithmetic held fixed: with -ffloat-store, scene_chain and
# scene_ragdoll are BYTE-IDENTICAL at i386 and LP64 over all 901 rows, and 100 runs of the plain
# -m64 ragdoll give one answer. scene_boxes_on_plane still differs from step 94 — a contact
# ORDER, not a value, and it predates the post-passes (proven.txt LP64-CONTACT-ORDER).
#
# The post-passes remain no-ops at 32-bit BY MEASUREMENT: all 145 objects compile byte-identical
# at -m32, and all 146 shipped sources compile BYTE-IDENTICALLY for wasm32 before and after —
# which is what keeps wasm32, i686 and armeabi-v7a exactly as they were, measured rather than
# argued.
#
# ⚠ NECESSARY, NOT SUFFICIENT, AND THE DIFFERENCE MATTERS ON THIS TARGET. x86-64 is the same DATA
# MODEL as arm64, not the same machine: it tolerates unaligned access where arm64 FAULTS, and the
# recovered code casts freely. The scenes are also OFFLINE — nothing at 64-bit has yet exercised a
# real match, the drop-in link or the .ka asset path, and `census_report.py` puts the whole
# recovery at about a quarter of its functions ever executed. `ptrwidth_check.sh` still counts 134
# truncation diagnostics across 32 objects at aarch64 (down from 7,714 across 95).
#
# ⚠ "NONE IS ON A PATH THE SCENES REACH" USED TO BE WRITTEN HERE AND IT WAS REFUTED. One of them
# was McdSphylPlaneIntersect reading a capsule's radius out of the low half of a pointer, 7,298
# times a run, in the ragdoll scene. What is true now is narrower and worth keeping narrow: none
# of the 134 that REMAIN moves any of the three scenes, which is a measurement over three scenes
# and not a proof over the corpus.
#
# ★★★ AND 64-BIT KARMA NOW PLAYS. This block used to read "⚠ ANDROID AT 64-BIT STILL CRASHES
# WITH KARMA ON — re-measured 2026-08-30 ... 501 log lines against 2153". That is no longer true,
# and leaving it would have been the third time this note was wrong. Measured 2026-09-01:
#
#   arm64-v8a (OnePlus 6)  ONS-Dria Onslaught, bots + vehicles + ragdolls, spectator:
#                          1397 s (23 min), 0 signals, frame advance verified every 60 s
#   armeabi-v7a            the SAME match on the 32-bit slice: 508 s, 0 signals
#   x86-64 Linux           no Karma frame left in an 8-gametype sweep
#
# ★ The armv7 arm is the attribution control the old note kept asking for, and it says the
# opposite of what that note concluded: BOTH WIDTHS PLAY. So "Karma is neutral at 32-bit and
# fatal at 64-bit" is dead. The last defect was a callback's `void *` context read at i386
# offsets, which no static gate in metoolkit-karma-decomp can see — its decomp/docs/STATE.md has
# the gdb dump. Karma is ON by default on every target now, including Android.
#
# ⚠ WHAT IS STILL NOT TESTED AT 64-BIT: Android x86_64 (in the APK, never run), and the Windows
# 64-bit binary (built with Karma, never run). KD_LP64_OK is still honoured as a way to silence
# this note in a script.
if(USE_KARMA_DECOMP AND UT_PTR_BYTES GREATER 4 AND NOT KD_LP64_OK)
  message(STATUS
    "USE_KARMA_DECOMP on a ${UT_PTR_BYTES}-byte-pointer target (arch "
    "${TARGET_ARCH}${ANDROID_ABI}) -- LP64 Karma, which plays: see the note above this line.")
endif()

if(NOT USE_KARMA_DECOMP AND NOT BUILD_KARMA_REF)
  # ⚠ SAY SO, EVERY TIME. `option()` reads an EXISTING cache before its default, so flipping
  # USE_KARMA_DECOMP to ON in 2026-08-27 does nothing to a build directory configured before
  # that — it silently keeps NO_KARMA and the only symptom is a game with no vehicles or
  # ragdolls. This line is how you find out; `rm -rf build-<preset>` is the fix.
  message(STATUS "Physics: NO_KARMA (no vehicles, no ragdolls) -- "
                 "USE_KARMA_DECOMP=OFF and BUILD_KARMA_REF=OFF. If that is not what you asked "
                 "for, this build directory has a stale cache: delete it and reconfigure.")
  add_definitions(-DNO_KARMA=1)
else()
  # ★ THE KARMA SDK AND THE RECOVERED LIBRARY ARE FETCHED, NOT VENDORED (ufront 2.44).
  #
  # Both used to live in this repository — Thirdparty/metoolkit (96 MB of MathEngine binaries) and
  # karma-decomp/ (292 commits of a decompilation project). Neither was engine code. They are now
  # https://github.com/10-p/metoolkit-karma-decomp, which carries the SDK, the recovered sources,
  # the toolchain that produces them and the gates that judge them.
  #
  # UNPINNED, DELIBERATELY. GIT_TAG is `main`: any new version of that repository is an improvement
  # in decompilation accuracy and this build wants it. If you need a fixed revision for a bisect,
  # set -DKARMA_DECOMP_GIT_TAG=<sha>, or point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at a
  # local checkout — which is also how you build offline, and how you test a change to the
  # recovered sources without pushing it.
  #
  # ⚠ DECLARED HERE, MADE AVAILABLE MUCH LATER, AND THE SPLIT IS LOAD-BEARING.
  # The -pthread block below says it in as many words: "every object in a
  # shared-memory link must carry the atomics/bulk-memory features, and that
  # includes the FetchContent'd gl4es and the Thirdparty C libraries. Hence it is
  # set HERE, ahead of FetchContent_MakeAvailable(GL4ES) and every
  # add_subdirectory below". The recovered Karma is one more such library, and
  # populating it at THIS point put its objects outside every global
  # add_compile_options that follows — measured: the wasm-perf build linked and
  # passed its whole suite with the Karma objects missing -pthread, which is the
  # kind of mismatch that works until it does not.
  include(FetchContent)
  # ufront 2.70: cache variables, so meta.json records the same repo@ref this configure fetched
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
  # METOOLKIT_DIR and the include paths are set with the MakeAvailable, further down.
  add_definitions(-DWITH_KARMA=1)
  # ★ ME_FORCE_CDECL — GLOBAL, and for the same reason WITH_KARMA is global: it changes the
  # CALLING CONVENTION of every metoolkit declaration, so a module that misses it disagrees with
  # every module that has it, at every Karma call, with nothing to diagnose it.
  #
  # MinGW-w64 predefines WIN32, so MeCall.h would otherwise declare the whole API __stdcall on
  # the `windows` preset. The recovered sources are cdecl — they were decompiled from the Linux
  # i386 build — so they need the override. See the note in the fetched metoolkit/include/MeCall.h.
  #
  # ⚠ AND THIS IS WHY IT IS GUARDED BY USE_KARMA_DECOMP RATHER THAN APPLIED UNCONDITIONALLY.
  # BUILD_KARMA_REF on Windows links `lib.rel/win32_single_msvcrt/*.lib`, which MathEngine built
  # WITH the __stdcall that MeCall.h declares — the archives' entry points are decorated for it
  # (`_McdNullCreate@4`). Forcing cdecl there compiles clean, links clean (the decoration is what
  # the engine asks for either way once the header agrees with itself) and corrupts the stack at
  # the first Karma call. The older text here said the Windows archives were "32-bit Linux ELF"
  # and so unusable; that was simply wrong, and it is the reason nobody tried for a year.
  if(USE_KARMA_DECOMP)
    add_definitions(-DME_FORCE_CDECL=1)
  endif()
endif()

# ufront 2.12 Part B: optional Pixomatic software renderer (the -SOFTWARERENDERER device). The open driver
# source is in-tree (PixoDrv/); the closed Pixomatic core lib is external via PIXOMATIC_DIR (see PixoDrv/
# CMakeLists.txt). Native only; builds in either backend (SDL2/gl4es or SDL 1.2/legacy).
option(USE_PIXOMATIC "Build the Pixomatic software render device (PixoDrv, -SOFTWARERENDERER)" OFF)

# ⚠ Pixomatic is a PREBUILT i386 ARCHIVE with no source — the same shape of problem Karma was,
# and the one thing in this tree that cannot follow a pointer-width change. At UT_ALLOW_64BIT it
# fails at the very last step with "pixomatic_linux.a ... incompatible with i386 output", after a
# full 200-TU build. Say so at configure time instead. The 64-bit vehicle uses gl4es, which is
# built from source; the software renderer is not needed to debug Karma.
if(USE_PIXOMATIC AND UT_PTR_BYTES EQUAL 8 AND NOT ANDROID AND NOT EMSCRIPTEN)
  message(FATAL_ERROR
    "USE_PIXOMATIC=ON is impossible at 64-bit, on EITHER platform, and there is no source to rebuild "
    "from: Thirdparty/Pixomatic/Lib/pixomatic_linux.a is a prebuilt 32-bit i386 archive, and the "
    "Windows side is pixomatic.lib (an import library) against a PE32 i386 pixomatic.dll. Configure "
    "with -DUSE_PIXOMATIC=OFF; 64-bit uses gl4es, which is built from source.")
endif()

# ★ THE -O2 SIGSEGV IN Source/PixoDrv WAS DIAGNOSED AND FIXED (ufront 2.58, 2026-09-11). It was never
# the optimiser's fault and never Pixomatic's: `utils_surface.h` typed the RAD surface shim's blit/stretch
# pointer `__stdcall` on every platform while `pixomatic_linux.a`'s entry points are cdecl, so each
# `scale_func(...)` call in scale_buffer() left esp 40 bytes low. -O0 kept a frame pointer and `leave`
# repaired it; -O2 omitted it and `ret` popped a framebuffer pointer — the "jump into heap data". Armed
# by Zoom2X=True, the ini default. The typedef is now stdcall only under _WIN32, and the fixed -O2 build
# renders and traces identically to -NULLRENDERER (see BUILD.md "ufront 2.58"). UT_PIXODRV_O0 in
# Source/PixoDrv/CMakeLists.txt is the owner's fallback pin for anything future, OFF.
#
# And the software renderer's mid-match crash (ONS-Torlan with bots, ~1-3 min, 5 of 8 runs) was an
# ENGINE -O2 defect the software renderer merely exposed: two `&Array(0)` null-reference bindings on
# empty TArrays in Source/Engine/Src/UnSkeletalMesh.cpp (ComputeSkinVerts, GetFrame) — UB the optimiser
# reasons from. Guarded; 4/4 passes at -O2 afterwards, engine -O0 4/4, that one file at -O1 3/3. The
# per-source knob that localised it is UT_ENGINE_TU_FILES in Source/Engine/CMakeLists.txt (BUILD.md "2.58").

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
#
# ⚠ THIS LINE IS WHY UT_ALLOW_64BIT DID NOTHING. The demotion block above honours the option,
# then this one re-applied -m32 unconditionally and the "64-bit vehicle" emitted `ELF 32-bit
# LSB relocatable, Intel i386` while announcing "Pointer width: 8 bytes". It even passed
# `-m32 -march=x86-64` together. Gate on UT_PTR_BYTES — the variable that already exists to be
# the width this build ACTUALLY produces — so the message and the objects cannot disagree.
if(NOT EMSCRIPTEN AND NOT ANDROID AND UT_PTR_BYTES EQUAL 4)
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
#
# ★★ ONE `-O` PER CONFIGURATION, AND EXACTLY ONE. Everything below states its optimisation level
# outright; nothing relies on a later flag beating an earlier one.
#
# ⚠ THAT USED NOT TO BE TRUE ON THE NATIVE PATH, AND IT SILENTLY COST EVERY NATIVE BUILD ITS
# OPTIMISER. CMake injects `CMAKE_<LANG>_FLAGS_<CONFIG>` — RelWithDebInfo is `-O2 -g -DNDEBUG` —
# AHEAD of anything `add_compile_options()` appends, and the compiler takes the LAST `-O`. The old
# native branch was a single `else()` marked "Match Phase 1's -O0 build for now" that appended
# `-O0 -g` to every configuration, so the command line read `-O2 -g -DNDEBUG … -m32 -O0 -g` and
# RelWithDebInfo — the build type every native and Windows preset asks for — compiled unoptimised.
# See BUILD.md's "EVERY NATIVE BUILD IS -O0" note, which documented this without fixing it.
#
# The per-config variables are blanked for the configurations handled here so there is one `-O` and
# one assertion decision on the line, rather than two of each and a last-wins race. ⚠ Blanking also
# drops the `-DNDEBUG` those variables carried, so each branch states NDEBUG explicitly — otherwise
# "clean up the flags" would quietly switch `assert()` back on across the engine.
#
# ⚠⚠ FIXING THIS CHANGES WHAT NATIVE BINARIES ARE, AND THAT IS NOT COSMETIC. BUILD.md's
# WIN32-PORT-3 records that at -O2 gcc sinks a dead null-page load onto the branch that guards it
# and at -O0 it does not — which is what turned one recovered Karma defect from dormant into a
# Windows startup crash. Native and Windows builds need re-validating against this, not assuming.
foreach(_cfg DEBUG RELWITHDEBINFO RELEASE)
  set(CMAKE_C_FLAGS_${_cfg} "")
  set(CMAKE_CXX_FLAGS_${_cfg} "")
endforeach()

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
  # ★ ufront 2.54: this is also THE SHIPPED `-debug` BINARY. It is what the launcher's "launch debug
  # binary" link loads, so it has to stay playable as well as symbolised — which is exactly the
  # trade -O2 -g was chosen for. Do not "improve" it to -O0; that is what UT_WEB_DEBUG is for.
  add_compile_options(-O2 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0 -g)
  add_definitions(-D_DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3 -g0)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
else()
  # RelWithDebInfo, and an unset build type. What every native/Windows preset actually asks for.
  add_compile_options(-O2 -g)
  add_definitions(-D_REENTRANT -D_NDEBUG=1 -DNDEBUG)
endif()

#-----------------------------------------------------------------------------
# x86 Instruction Sets (per makefile-header)
#-----------------------------------------------------------------------------

if(TARGET_IS_X86 AND NOT EMSCRIPTEN AND NOT ANDROID)
  # ⚠ `-march=pentium` IS AN x86-32 CPU AND cc1plus REFUSES IT AT -m64
  # ("CPU you selected does not support x86-64 instruction set"), so the
  # UT_ALLOW_64BIT vehicle has to ask for a 64-bit baseline instead. This is the
  # makefile-header's instruction-set line and it stays exactly as it was for
  # every shipping target.
  if(UT_ALLOW_64BIT AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_options(-march=x86-64 -mtune=generic -msse2)
  else()
    add_compile_options(-march=pentium -mtune=pentiumpro -msse -mmmx)
    # ★ ufront 2.58: UT_FPMATH_SSE is the ktrace CONTROL arm (`native-sse` preset). A 32-bit build
    # otherwise runs its doubles on x87 at 80-bit and can never match the 64-bit (SSE) reference:
    # x87 yields trace md5 81b06b0cf715 where the reference is c31ed77b7323. Until 2.58 this was an
    # ad-hoc flag that never reached the cache — measured 2026-09-11: the build-b32-sse binary was
    # SSE by its DWARF producer string while its own CMakeCache and flags.make said x87, so any
    # rebuild of that directory would have silently regressed the control. A cached option and a
    # preset are the only form of that knowledge that survives a rebuild.
    if(UT_FPMATH_SSE)
      add_compile_options(-msse2 -mfpmath=sse)
      message(STATUS "UT_FPMATH_SSE: 32-bit float math on SSE2 (the ktrace control arm)")
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

if(NOT EMSCRIPTEN AND NOT ANDROID AND UT_PTR_BYTES EQUAL 4)
  add_link_options(-m32)
endif()

#-----------------------------------------------------------------------------
# Dependencies
#-----------------------------------------------------------------------------

# SDL2 via FetchContent (per D-04). Only the gl4es backend uses SDL2; the legacy backend uses the
# vendored SDL 1.2 .so, and Emscripten gets SDL2 from the -sUSE_SDL=2 port.
include(FetchContent)

# ---------------------------------------------------------------------------
# ut_provide_samplerate_linker_name — SDL2's `-lsamplerate` on a 64-bit Linux
# build where only the i386 dev package is installed.
#
# ⚠ THE FAILURE LOOKS NOTHING LIKE ITS CAUSE. `samplerate.h` is architecture
# INDEPENDENT and lives in /usr/include, so `libsamplerate0-dev:i386` alone
# satisfies SDL2's `check_include_file(samplerate.h)`; SDL then appends a bare
# `-lsamplerate` and the 64-bit link dies with
#
#     /usr/bin/ld: cannot find -lsamplerate: No such file or directory
#
# because only the DEV package ships the unversioned `libsamplerate.so` that
# `-l` resolves, and the amd64 one is not installed. The runtime library IS
# there — `libsamplerate.so.0` — under a name the linker will not look for.
#
# ★ THIS EXISTED FOR A WEEK AS `-L/tmp/kd_lib64`, A HAND-MADE SYMLINK IN A
# DIRECTORY THAT DOES NOT SURVIVE A REBOOT. Every 64-bit build after one would
# fail with the message above and nothing anywhere connected it to a wiped /tmp.
# Doing it here makes the build self-sufficient: no root, no apt, nothing
# outside the build tree — and if the runtime library is missing too, the build
# says which package to install rather than failing at the link.
function(ut_provide_samplerate_linker_name)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    return()
  endif()
  # Ask the linker itself rather than guessing a path: `-lsamplerate` is exactly
  # what SDL2 will emit, so this probe runs the thing that would actually fail.
  execute_process(COMMAND ${CMAKE_C_COMPILER} -lsamplerate -o /dev/null -xc /dev/null
                  RESULT_VARIABLE _sr_rc OUTPUT_QUIET ERROR_QUIET)
  if(_sr_rc EQUAL 0)
    return()                                    # the dev package is installed
  endif()
  find_library(UT_SAMPLERATE_SONAME NAMES libsamplerate.so.0
               PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib)
  if(NOT UT_SAMPLERATE_SONAME)
    message(STATUS
      "libsamplerate not usable at 64-bit -- disabling SDL2's use of it. "
      "For the better resampler: sudo apt-get install libsamplerate0-dev:amd64")
    set(SDL_LIBSAMPLERATE OFF CACHE BOOL "" FORCE)
    return()
  endif()
  set(_dir "${CMAKE_BINARY_DIR}/complib")
  file(MAKE_DIRECTORY "${_dir}")
  file(CREATE_LINK "${UT_SAMPLERATE_SONAME}" "${_dir}/libsamplerate.so" SYMBOLIC)
  # link_directories() is directory-scoped and SDL2 is added as a subdirectory
  # from HERE, so it reaches SDL's own link; the linker flag covers the targets
  # that record a link line of their own.
  link_directories("${_dir}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${_dir}" PARENT_SCOPE)
  message(STATUS "libsamplerate: no amd64 dev package -- linking through "
                 "${_dir}/libsamplerate.so -> ${UT_SAMPLERATE_SONAME}")
endfunction()

if(NOT EMSCRIPTEN AND UT_GFX_BACKEND STREQUAL "gl4es")
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.x
    GIT_SHALLOW    TRUE
  )
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  # ufront 2.34 — SDL2main is needed on Windows and only on Windows. SDL.h renames the app's `main` to
  # `SDL_main`, and on Windows it is SDL2main that supplies the real WinMain which calls it; without the
  # library the link ends in "undefined reference to `WinMain@16`" with no mention of SDL. Everywhere else
  # SDL2main is an empty shim we do not want in the way.
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
      # ufront 2.33 (Windows) — SDL2 selects its own Windows video/audio backends. The X11 forcing below
      # is a Linux -m32 cross-compile workaround; applying it here would send SDL2 hunting for X11 libs
      # that do not exist on a Windows target.
      FetchContent_MakeAvailable(SDL2)
    elseif(UT_PTR_BYTES EQUAL 4)
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
      # UT_ALLOW_64BIT — a native x86_64 build. Pointing SDL2 at /usr/lib/i386-linux-gnu here is
      # the mirror image of the bug the demotion block warns about: the probes link 64-bit objects
      # against i386 libs, every check silently fails, SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS
      # goes undefined, and SDL_x11xinput2.h emits a fallback XGenericEventCookie that collides
      # with the real one in the (arch-independent) Xlib.h — six TUs, ten hard errors, and the
      # build stops before it can link. The amd64 X11 dev packages are installed, so let SDL2's
      # own find_package do its job.
      set(SDL_X11 ON CACHE BOOL "" FORCE)
      set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
      set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
      ut_provide_samplerate_linker_name()
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
# openal-soft (ufront 2.61 / 2.70) — ONE fork on every platform that builds it:
# https://github.com/10-p/openal-soft-web `ufront/web` = openal-soft 1.25.2 + the Web Audio backend + the
# wasm build patches. The web block further down builds it STATIC into the wasm; Android (2.61) and native
# Linux (2.70) build it SHARED and load it late. The cache variables are declared once, here, so the three
# fetches cannot drift apart and meta.json records exactly what was fetched (cmake/ArtifactDeps.cmake).
set(OPENALSOFT_GIT_REPOSITORY "https://github.com/10-p/openal-soft-web.git"
    CACHE STRING "openal-soft git remote; its ufront/web branch carries the Web Audio backend")
set(OPENALSOFT_GIT_TAG "ufront/web" CACHE STRING "openal-soft git ref (a branch: a rebuild takes its tip)")

# ufront 2.70 — native Linux ships its own openal-soft too. The Ubuntu libopenal (1.25.1) is built without
# EAX, so ALAudio's EAX path — on everywhere since 2.61 — logged "EAX off (this OpenAL has no EAX)" on the
# one platform where the library was not ours. Same shape as Android: a SHARED libopenal.so that ALAudio
# dlopens at audio-init (never DT_NEEDED: the engine's global operator new is exported and GMalloc does not
# exist yet at process start), copied beside the binary by SDLLaunch's POST_BUILD, found through the
# executable's rpath '.' when run from a System/ directory.
option(UT_BUNDLED_OPENAL "Native Linux: build openal-soft (the ufront fork, EAX on) and put libopenal.so beside the binary, instead of dlopen'ing whatever libopenal the system has" ON)
set(UT_LINUX_OPENALSOFT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID AND NOT EMSCRIPTEN AND UT_BUNDLED_OPENAL)
  set(UT_LINUX_OPENALSOFT ON)
endif()

if(ANDROID OR UT_LINUX_OPENALSOFT)
  # openal-soft is modern C++ (1.25.2: C++20) whose static-init filter tables (core/bsinc_tables.cpp) use
  # constructs the engine's global build environment miscompiles: the engine forces CMAKE_CXX_STANDARD 14
  # (legacy UT2004 tree) + -fdelayed-template-parsing (clang MSVC-compat), both of which a FetchContent'd
  # subproject inherits -> garbage array sizes -> a null-deref (memset) crash in openal's global
  # constructors at dlopen (Android, 2.10). Build openal-soft as C++20 with the compat flag dropped;
  # save/restore so the rest of the engine keeps gnu++14. openal-soft pins CXX_STANDARD 20 on its own
  # targets (that is what its objects compile as); the directory-level standard covers its configure-time
  # feature checks. ufront 2.70: Android converged onto the fork from an upstream 1.23.1 pin, so it runs the
  # same library as the web — with EAX, which the pin had but which was never built or run there.
  set(_UT_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_UT_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
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
  endif()
  # Linux: the backends are whatever the host's headers allow (PulseAudio and ALSA here; PipeWire when its
  # headers are present), each dlopen'd at run time (ALSOFT_DLOPEN), so the .so has no audio DT_NEEDED.
  set(ALSOFT_EAX ON CACHE BOOL "" FORCE)                  # ufront 2.61: EAX for ALAudio on every platform
  set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
  # openal-soft's cmake_minimum_required predates CMake 4.x's floor; force a modern policy (as GL4ES does).
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(OpenALSoft)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_CXX_FLAGS "${_UT_SAVED_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD "${_UT_SAVED_CXX_STANDARD}")
  if(NOT TARGET OpenAL)
    message(FATAL_ERROR "openal-soft: expected target 'OpenAL' was not created")
  endif()
  message(STATUS "openal-soft (shared, EAX): ${OPENALSOFT_GIT_REPOSITORY}@${OPENALSOFT_GIT_TAG}")
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
  # 64-bit Android does not get -m32, and neither does the UT_ALLOW_64BIT vehicle.
  #
  # ⚠ THIS ONE IS A `CACHE ... FORCE`, so it does not merely build gl4es 32-bit — it writes
  # `-m32` into CMAKE_C_FLAGS for the WHOLE PROJECT, taking zlib, ogg, vorbis and Speex with it.
  # At UT_ALLOW_64BIT that produced an i386 libGL.a in an otherwise x86-64 tree.
  if(NOT EMSCRIPTEN AND NOT ANDROID AND UT_PTR_BYTES EQUAL 4)
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
  # ufront 2.34 — gl4es names its library target after what it IMPERSONATES, and on Windows that is
  # `OPENGL32`, not `GL` (it builds an opengl32 replacement there). Everything below refers to one handle,
  # so normalise the name once. Without this the Windows configure dies in GL4ESDrv with "gl4es::GL is
  # missing" — which reads like "the backend is wrong" and is really "the target has another name".
  if(TARGET GL)
    set(_UT_GL4ES_TARGET GL)
  elseif(TARGET OPENGL32)
    set(_UT_GL4ES_TARGET OPENGL32)
  endif()
  if(TARGET ${_UT_GL4ES_TARGET})
    set_target_properties(${_UT_GL4ES_TARGET} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    # Unambiguous, namespaced handle so GL4ESDrv links the translation layer's libGL.a and can
    # never silently fall back to the system -lGL (the "two versions of OpenGL" hazard).
    if(NOT TARGET gl4es::GL)
      add_library(gl4es::GL ALIAS ${_UT_GL4ES_TARGET})
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
      target_compile_options(${_UT_GL4ES_TARGET} PRIVATE -O2)
      message(STATUS "GL4ES: forcing -O2 in the -O0 web build (at -O0 + Asyncify, "
                     "generateInstruction exceeds V8's 50000-local limit and the module will not compile)")
    endif()
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

#-----------------------------------------------------------------------------
# Karma: populate the fetched SDK + recovered library.
#
# ★ THIS IS DELIBERATELY THE LAST THING BEFORE add_subdirectory(), so the
# recovered Karma is compiled with exactly the same global options as every
# other module — most importantly -pthread on the threaded web build, where an
# object without the atomics/bulk-memory features is a shared-memory link
# mismatch that does not announce itself.
#
# It was populated up in the option block once (ufront 2.44) and the wasm build
# linked, ran, and passed its whole browser suite with the Karma objects missing
# -pthread. That is not evidence it was fine; it is what this class of bug looks
# like before it bites.
#-----------------------------------------------------------------------------
# The LEGACY arm populates the same repository, and stops there. It wants the SDK that repository
# carries — the public headers and the shipped archives — and none of the recovered sources, so it
# never reaches the -O0 pin block below (which exists to constrain code this arm does not compile).
if(BUILD_KARMA_REF)
  FetchContent_MakeAvailable(MetoolkitKarmaDecomp)
  # ...and it must not BUILD them either. MakeAvailable adds that directory's targets to `all`, so
  # without this the legacy arm compiles 146 recovered C sources on every CI run and links none of
  # them. Same get_property walk the -O0 pin below uses, so both stay keyed off one mechanism.
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

  # ★★ THE RECOVERED KARMA C IS PINNED TO -O0 ON NATIVE/WINDOWS, WHILE THE ENGINE AROUND IT KEEPS THE
  # OPTIMISATION ufront 2.54 RESTORED. Measured, not assumed (ufront 2.55, `test-karma-1`, 40 frames,
  # Onslaught, KTRACECONTACTS, x86-64):
  #
  #     everything -O0 .................. 600 body rows, K=1396, md5 c31ed77b7323   <- the reference
  #     Karma C -O0 + engine C++ -O2 .... 600 body rows, K=1396, md5 c31ed77b7323   <- byte-identical
  #     everything -O2 .................. 543 body rows, K=1074, md5 179ace20fb15   <- DIVERGES
  #
  # So the engine is innocent: optimising it changes nothing. Optimising the RECOVERED code changes
  # the simulation — and not by a rounding digit, the traced body-row count itself drops. That breaks
  # `decomp/test/ut2004/ktrace_gate.sh`, whose whole method is a byte-for-byte trajectory comparison
  # against md5 c31ed77b7323 / K=1396, and which has caught real defects by being exact.
  #
  # ★★ DIAGNOSED (ufront 2.58, 2026-09-11) — THE TRANSFORMATION IS NAMED, AND IT IS ONE FILE. GCC's
  # value-range propagation (-ftree-vrp, on at -O2, off at -O1/-Og) folds the exit conditions of the
  # two 3x3 matrix loops in metoolkit_decomp/src/MdtKea/keaCalcIworldandNonInertialForceandVhmf_vanilla.c
  # (lines 78-103): Ghidra flattened `m[i][j]` into `m1[0][iVar9 + k]` with iVar9 = 0,3,6 on a local
  # declared `MeReal m1[3][3]`, so iterations 2 and 3 subscript past the inner array — undefined in C,
  # exactly what the machine code does, and GCC says so once `-w` is lifted: "iteration 1 invokes
  # undefined behavior [-Waggressive-loop-optimizations]". It cuts each loop to ONE iteration, rows 2-3
  # of the world inertia tensor are never written, and the first body with a constraint (the Scorpion,
  # frame 1) is solved wrong. Evidence, all on test-karma-1 / 40 frames / -KTRACECONTACTS:
  #     every probe at -O2 (-fwrapv, -ftrivial-auto-var-init=zero, -fno-inline, -fno-tree-dse,
  #       -ffp-contract=off, -fno-ipa-*) ............................ still 543/1074 (DIVERGES)
  #     -O1, -Og, -O2 -fno-tree-vrp ................................. 600/1396 c31ed77b7323
  #     complement bisect over 146 files (UT_KARMA_O2_FILES) ......... names that one file, alone
  #     that file at -O2 -fno-tree-vrp, rest pinned .................. c31ed77b7323
  #     that file's 10 subscripts respelled `((MeReal *)m1)[k]`, ALL 146 files at -O2 .. c31ed77b7323
  #     the SDK's amd64 build of the function (lib.chk/win_amd64_whidbey_single) .. 12x `cmpl $3`: 3 iterations
  #     the same respelling under the shipping wasm flags (-O3 -g0) .. object BYTE-IDENTICAL
  # The web never showed it: the shipped -O3 wasm traces 600/1396 (13 of 15 bodies bit-identical to the
  # reference, the rest at the libm floor) — clang does not draw that inference. The same flattened
  # shape exists in MdtBcl.c (9 sites) and ReadWriteKeaInputToFile.c (1), latent on this map.
  # ⚠ NOT strict aliasing (measured: kt-noalias == kt-o2-64a) and not FMA (no FMA ISA on any arm).
  # The pin stays until the owner decides whether to land the generator repair; UT_KARMA_O2_FILES below
  # is how any of the rows above is re-measured.
  #
  # ★ WHY THIS COSTS LESS THAN IT LOOKS (owner, 2026-09-07): this source was DECOMPILED FROM AN
  # ALREADY-OPTIMISED BUILD. MathEngine shipped Karma compiled, and the recovery reconstructs C from
  # that object code — so the inlining, strength reduction and scheduling are already baked into the
  # source's shape. Re-optimising an optimiser's output is expected to buy little, which makes -O0
  # here a cheap price rather than a sacrifice. That is a prediction, not a measurement; if anyone
  # wants the speed back, MEASURE the two first.
  #
  # ⚠ THE WEB IS DELIBERATELY NOT INCLUDED. wasm-perf builds this code at -O3 and the dev/`-debug`
  # wasm at -O2, and always has — 2.54's `else()` branch only ever caught the native configurations.
  # The web is validated by its own E2E (170/170 at ufront 2.55) and by the owner playing it daily,
  # in exactly that optimised state. Changing it would alter the shipped binary and invalidate that
  # evidence to chase a native-only gate, so it stays as it is.
  #
  # ★ ufront 2.58 — two knobs so the pin can be MEASURED rather than assumed (BUILD.md "2.58"):
  #   UT_KARMA_O2_FILES  a regex over decomp source BASENAMES; matching sources get UT_KARMA_O2_OPTIONS
  #                      (a per-source COMPILE_OPTIONS lands after the target's -O0; last -O wins).
  #                      '.*' is the "everything -O2" arm (543 rows / K=1074 / 179ace20fb15); a
  #                      narrower regex is one round of the complement bisect. Empty = the pin as shipped.
  #   UT_KARMA_O2_OPTIONS the options those sources get, default "-O2". A probe is one reconfigure:
  #                      "-O1", "-O2;-fwrapv", "-O2;-ftrivial-auto-var-init=zero", "-O2;-fsanitize=undefined"
  #                      (that one also needs -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined). Scoped to
  #                      the matching decomp sources only, so the engine's own C never changes.
  #   UT_KARMA_PIN_WEB   lift the NOT EMSCRIPTEN guard so the web arm can be built with the pin.
  #                      Default OFF — the shipped wasm stays -O3 unless a measurement says otherwise.
  set(UT_KARMA_O2_FILES "" CACHE STRING
      "regex over metoolkit_decomp source basenames to compile at -O2 despite the pin (bisect aid; empty = all pinned)")
  set(UT_KARMA_O2_OPTIONS "-O2" CACHE STRING
      "compile options (;-list) for the sources UT_KARMA_O2_FILES matches (probe aid; default -O2)")
  option(UT_KARMA_PIN_WEB "Also pin the recovered Karma C to -O0 on Emscripten (measurement aid; the shipped wasm is -O3)" OFF)
  if(NOT EMSCRIPTEN OR UT_KARMA_PIN_WEB)
    get_property(_md_targets DIRECTORY "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit_decomp"
                 PROPERTY BUILDSYSTEM_TARGETS)
    set(_md_o2 0)
    foreach(_md_t IN LISTS _md_targets)
      # PRIVATE target options land AFTER the directory-level `add_compile_options(-O2 -g)` above,
      # and the compiler takes the last -O — the same last-wins rule the 2.54 bug relied on, used
      # deliberately here.
      target_compile_options(${_md_t} PRIVATE -O0)
      if(NOT UT_KARMA_O2_FILES STREQUAL "")
        get_target_property(_md_srcs ${_md_t} SOURCES)
        get_target_property(_md_dir ${_md_t} SOURCE_DIR)
        if(_md_srcs)
          foreach(_md_s IN LISTS _md_srcs)
            if(NOT IS_ABSOLUTE "${_md_s}")
              set(_md_s "${_md_dir}/${_md_s}")
            endif()
            get_filename_component(_md_b "${_md_s}" NAME)
            if(_md_b MATCHES "\\.c$" AND _md_b MATCHES "${UT_KARMA_O2_FILES}")
              # Source-file options come after the target's, so this -O2 beats the -O0 above.
              set_source_files_properties("${_md_s}" TARGET_DIRECTORY ${_md_t} PROPERTIES COMPILE_OPTIONS "${UT_KARMA_O2_OPTIONS}")
              math(EXPR _md_o2 "${_md_o2}+1")
            endif()
          endforeach()
        endif()
      endif()
    endforeach()
    list(LENGTH _md_targets _md_n)
    if(EMSCRIPTEN)
      message(STATUS "Karma decomp: ${_md_n} target(s) pinned to -O0 ON THE WEB (UT_KARMA_PIN_WEB; ufront 2.58 measurement arm)")
    else()
      message(STATUS "Karma decomp: ${_md_n} target(s) pinned to -O0 (engine keeps -O2; ufront 2.55)")
    endif()
    if(_md_o2 GREATER 0)
      message(STATUS "Karma decomp: ${_md_o2} source(s) compiled with '${UT_KARMA_O2_OPTIONS}' via UT_KARMA_O2_FILES='${UT_KARMA_O2_FILES}' (ufront 2.58 probe/bisect arm)")
    endif()
  endif()
endif()

# The SDK's location and its public headers — needed by BOTH Karma arms, because the recovered
# sources reimplement what the archives contain and share their headers verbatim.
if(USE_KARMA_DECOMP OR BUILD_KARMA_REF)
  set(METOOLKIT_DIR "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit" CACHE PATH "metoolkit (Karma SDK) root: include/ + lib.{chk,rel}/")
  # Only the legacy arm ever reads this, and that arm is Linux-only (see BUILD_KARMA_REF's note);
  # the `win32_single_msvcrt` variant beside it links but cannot be used, which is recorded there
  # so that pointing this at it looks like the dead end it is.
  set(METOOLKIT_LIB_SUBDIR "lib.rel/linux_single_gcc3.2" CACHE STRING "metoolkit lib variant (rel/chk + toolchain)")
  # Global, exactly as the ORIGINAL v3369 build did: the Epic makefile-header applies
  # -I$(METOOLKIT_INC) to EVERY module's compile, and a module that misses it while another has it
  # disagrees about AActor/ULevel/ASVehicle's Karma members -- which is an in-game vtable crash.
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
# openal-soft on the WEB (ufront 2.61) -- replaces Emscripten's built-in OpenAL (-lopenal / libopenal.js).
#
# Why: libopenal.js is stereo-only by construction (every source goes through a PannerNode), has no EFX,
# and all 93 of its functions are __proxy:'sync', so every AL call from the engine thread is a blocking
# round trip to the browser main thread. openal-soft is the same API, mixed in wasm on its own pthread
# and played by an AudioWorklet (the `web` backend on the ufront/web branch, alc/backends/web.cpp).
#
# Placed here, after the global -pthread compile option and before the modules, for the same reason as
# Karma above: an object without the atomics/bulk-memory features is a shared-memory link mismatch.
#
# ★ THREE THINGS THIS BLOCK HAS TO GET RIGHT (ufront 2.61 §1):
#  1. The engine's C++ flags are not openal-soft's. -fdelayed-template-parsing miscompiled 1.23.1's static
#     init on Android (see the Android block above); openal-soft pins its own CXX_STANDARD 20 per target.
#  2. EXCEPTIONS. openal-soft reports every AL error by THROWING and catching it at the API boundary
#     (context->throw_error). The shipping link had catching disabled, which turns the first AL error into
#     an abort -- measured on a 20-line repro. Wasm-native exceptions cannot be used: Binaryen's Asyncify
#     pass fails the link ("__asyncify_get_call_index does not exist"). So openal-soft alone is compiled
#     with Emscripten's JS exceptions (-fexceptions) and SDLLaunch links with catching enabled; the engine's
#     own objects stay -fignore-exceptions and gain no invoke_ trampolines.
#  3. SIMD. Its hand-written SSE mixers compile through Emscripten's translation headers with
#     -msimd128 -msse4.1: HRTF costs 1.04x native instead of 2.55x (2.61 §0.2). NEON must be OFF -- its
#     header check passes on Emscripten and the runtime would then prefer the NEON path.
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
  set(_UT_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_UT_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
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
  # EAX 2.0-5.0 (ufront 2.61): ALAudio's EAX path now builds on every platform and asks the library at run
  # time. openal-soft's own default is ON only for Windows -- a CMake default, not a platform limit.
  set(ALSOFT_EAX ON CACHE BOOL "" FORCE)
  set(ALSOFT_BACKEND_WEB ON CACHE BOOL "" FORCE)
  set(ALSOFT_REQUIRE_SSE4_1 ON CACHE BOOL "" FORCE)  # a build that silently fell back to the C mixers is 2.5x slower
  set(ALSOFT_EMBED_HRTF_DATA ON CACHE BOOL "" FORCE)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(OpenALSoft)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_CXX_FLAGS "${_UT_SAVED_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${_UT_SAVED_C_FLAGS}")
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

# Viewport + render driver selection -- HARD SPLIT. Each backend builds exactly one viewport
# driver and one render driver; the unused modules are not compiled, linked, or registered.
# The UT_HAS_* vars propagate to SDLLaunch below.
if(EMSCRIPTEN)
  # Emscripten forced stack: SDL2 + GL4ES + WebRTC (matches the shipping web build).
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  add_subdirectory(Source/WebRTCDrv)
  # "Proxy networking" (ufront 2.79): a client-only net driver that joins an ordinary NATIVE server through
  # a WSS<->UDP game proxy. EXPERIMENTAL, and a package of its own — it shares nothing with WebRTCDrv.
  # -DUT_WITH_PROXYNET=OFF removes the package, its JS shim and its registration; P2P is unaffected.
  option(UT_WITH_PROXYNET "Build ProxyNetDrv (join native servers through a game proxy; web only)" ON)
  if(UT_WITH_PROXYNET)
    add_subdirectory(Source/ProxyNetDrv)
  endif()
  set(UT_HAS_SDL2DRV ON)
  set(UT_HAS_GL4ESDRV ON)
else() # gl4es
  # GL4ES stack: SDL2 + GL4ES translation layer + desktop OpenGL (ufront 2.34 — see below).
  add_subdirectory(Source/SDL2Drv)
  add_subdirectory(Source/GL4ESDrv)
  set(UT_HAS_SDL2DRV ON)
  set(UT_HAS_GL4ESDRV ON)
  # ★ ufront 2.34 — OpenGLDrv joins the SDL2 stack, so ONE binary carries both GL render devices and the
  # user picks at startup (-OPENGLRENDERER / -GL4ESRENDERER). The old comment above said the two "must
  # NEVER share a binary" for two reasons; neither survives:
  #   - "SDL 1.2 and SDL 2 export clashing symbols" — true, and irrelevant now that SDL 1.2 is retired
  #     and both drivers sit on SDL2.
  #   - "OpenGLDrv (Mesa GL/gl.h) and GL4ESDrv (gl4es GL/gl.h) cannot coexist in one translation unit" —
  #     literally true and already contained: each sets its include directories PRIVATE, so neither ever
  #     sees the other's header. They are never in one TU.
  # Nor do their symbols collide: OpenGLDrv links no GL library at all (it binds every entry point
  # through a pointer table filled by SDL_GL_GetProcAddress), and gl4es compiles -fvisibility=hidden on
  # non-Windows so its glXxx never reaches the dynamic symbol table. Only one device is ever LIVE — SDL
  # creates one context per window, and GRenderSwitches pairs each device with its viewport.
  if(NOT EMSCRIPTEN AND NOT ANDROID)
    add_subdirectory(Source/OpenGLDrv)
    set(UT_HAS_OPENGLDRV ON)
  endif()
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
# ufront 2.33: nor on Windows yet — UCC.cpp's Windows path wants the UNICODE-era winToUNICODE/
# winGetSizeUNICODE helpers that only UnVcWin32.h provides. The GAME does not need it, so it is left out
# of the first Windows cut rather than half-ported.
if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT MINGW)
  add_subdirectory(Source/UCC)
endif()


# Android JNI process-prep bridge (2.10b): a tiny standalone .so, loaded by GameActivity's static
# block and called before super.onCreate()/SDL_main, that sets UT2004_ANDROID_BASEDIR + chdir's to
# the on-storage System dir so the engine reads the game tree in place. Android NDK target only.
if(ANDROID)
  add_library(ut2004_android_bridge SHARED ${CMAKE_SOURCE_DIR}/Android/app/src/main/cpp/android_bridge.cpp)
  target_link_libraries(ut2004_android_bridge android log)
endif()
