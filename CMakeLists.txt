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

option(UT_ALLOW_64BIT "Skip the -m32 demotion on x86_64 Linux. NOT a shipping target — a debugging vehicle for LP64 defects." OFF)
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
# ★ -DUT_ALLOW_64BIT=ON SKIPS THE DEMOTION, AND IT EXISTS FOR ONE MEASUREMENT.
# The Android x86_64 build ALREADY runs this engine at 64-bit — with Karma off it
# reaches UT2k4MainMenu.Opened() off the retail packages — so "the engine cannot
# do LP64" is refuted, and the only thing that crashes there is Karma. What is
# missing is a way to DEBUG that: on Android the engine installs its own SIGSEGV
# handler, so there is no tombstone and no backtrace, and there is no gdb, no
# ASan and no valgrind. An x86-64 LINUX build is the same pointer width with all
# of those.
#
# ⚠ IT IS NOT A SHIPPING TARGET. D-05 stands: the shipped Linux binary is 32-bit.
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

# Karma: disabled by default (per D-09) — the modern web/native stacks don't use it. ufront 2.12 Part B
# restores the BUILD_KARMA_REF cmake path (linking the prebuilt 32-bit metoolkit libs) to exercise it as an
# optional third native binary; the `karma-ref` preset sets it. NO_KARMA stays defined unless it's ON.
option(BUILD_KARMA_REF "Build the 32-bit Karma reference (links the prebuilt metoolkit static libs)" OFF)

# USE_KARMA_DECOMP — the RECOVERED Karma, built from source in `metoolkit_decomp/src`/.
# ON BY DEFAULT since 2026-08-27: this is now the physics for every target that can take it.
#
# This is the point of the whole metoolkit-karma-decomp effort: MathEngine shipped Karma as binary-only
# 32-bit x86 archives, so BUILD_KARMA_REF below can never work off x86 and the web and Android
# builds were therefore always NO_KARMA — no vehicles, no ragdolls. The recovered sources are
# ordinary portable C, so they compile and RUN wherever the pointer width is right.
#
# It takes the same code path as BUILD_KARMA_REF — same global -DWITH_KARMA=1, same metoolkit
# INCLUDE dirs, because the public headers are portable C and are not what was missing — and
# differs only in what gets LINKED: a CMake target built from our sources instead of a glob of
# gcc-3.2 archives. Setting it implies BUILD_KARMA_REF's compile side, so ask for one or the
# other, never both. `legacy-karma` is the preset that asks for the other.
#
# EVIDENCE FOR THE DEFAULT, so it is not taken on trust: the i386 drop-in plays a full match
# against a metoolkit with every shipped member deleted, and the wasm build passes the whole
# ut2004 browser suite (51/51) equal to a NO_KARMA control built from the same tree.
# metoolkit-karma-decomp `decomp/proven.txt`, entries WASM-SIGMISMATCH and WASM-INDIRECT-SIGS.
option(USE_KARMA_DECOMP "Link the RECOVERED Karma (`metoolkit_decomp/src`) instead of the prebuilt metoolkit archives" ON)
if(USE_KARMA_DECOMP AND BUILD_KARMA_REF)
  message(FATAL_ERROR
    "USE_KARMA_DECOMP and BUILD_KARMA_REF are two implementations of the same API and both "
    "define its symbols. Pick one: BUILD_KARMA_REF for the shipped 32-bit x86 archives, "
    "USE_KARMA_DECOMP for the recovered sources.")
endif()

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

if(NOT BUILD_KARMA_REF AND NOT USE_KARMA_DECOMP)
  # ⚠ SAY SO, EVERY TIME. `option()` reads an EXISTING cache before its default, so flipping
  # USE_KARMA_DECOMP to ON in 2026-08-27 does nothing to a build directory configured before
  # that — it silently keeps NO_KARMA and the only symptom is a game with no vehicles or
  # ragdolls. This line is how you find out; `rm -rf build-<preset>` is the fix.
  message(STATUS "Physics: NO_KARMA (no vehicles, no ragdolls) -- "
                 "USE_KARMA_DECOMP=OFF. If that is not what you asked for, this build "
                 "directory has a stale cache: delete it and reconfigure.")
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
  # set GIT_TAG on the command line, or point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at a
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
  FetchContent_Declare(
    MetoolkitKarmaDecomp
    GIT_REPOSITORY https://github.com/10-p/metoolkit-karma-decomp.git
    GIT_TAG        main
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
  # i386 build — and MathEngine's Windows archives are not linkable here in any case (they are
  # 32-bit Linux ELF). See the note in the fetched metoolkit/include/MeCall.h.
  #
  # Only for USE_KARMA_DECOMP. BUILD_KARMA_REF is Linux-only by construction — the shipped
  # members are 32-bit Linux ELF — so WIN32 is never defined on that path and MEAPI is already
  # empty there; setting it would be a no-op that invited someone to wonder what it meant.
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
    "USE_PIXOMATIC=ON is impossible at 64-bit: Thirdparty/Pixomatic/Lib/pixomatic_linux.a is a "
    "prebuilt 32-bit i386 archive and there is no source to rebuild. Configure with "
    "-DUSE_PIXOMATIC=OFF for the UT_ALLOW_64BIT vehicle.")
endif()

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
  # ⚠ `-march=pentium` IS AN x86-32 CPU AND cc1plus REFUSES IT AT -m64
  # ("CPU you selected does not support x86-64 instruction set"), so the
  # UT_ALLOW_64BIT vehicle has to ask for a 64-bit baseline instead. This is the
  # makefile-header's instruction-set line and it stays exactly as it was for
  # every shipping target.
  if(UT_ALLOW_64BIT AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_options(-march=x86-64 -mtune=generic -msse2)
  else()
    add_compile_options(-march=pentium -mtune=pentiumpro -msse -mmmx)
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
if(BUILD_KARMA_REF OR USE_KARMA_DECOMP)
  FetchContent_MakeAvailable(MetoolkitKarmaDecomp)
  set(METOOLKIT_DIR "${metoolkitkarmadecomp_SOURCE_DIR}/metoolkit" CACHE PATH "metoolkit (Karma SDK) root: include/ + lib.{chk,rel}/")
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
