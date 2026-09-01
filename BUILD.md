# Building engine-ut2004 (native + WebAssembly)

This is the **private engine repo** for the UT2004 (UE2.5) web port — engine source only. Game
assets (~5.8 GB: maps, textures, meshes, sounds, animations, compiled `.u` packages) live **out of
tree** at `$UT_ASSETS_DIR` (default `/home/ion/ut2004-assets`) and are never tracked here.

> Status: the heavy browser glue has been **extracted** to the public monorepo (`@ufront/glue`,
> Phase 1 steps 1.3/1.4) and is injected at runtime — the WASM build bakes only the thin `webrtc_*`
> shim (`vendor/glue/webrtc-shim.js`). As of **step 1.6a** the host page is **un-baked** too: the
> link drops `--shell-file` and uses `SUFFIX .js`, so the build emits a **bare loader**
> (`SDLLaunch.{js,wasm,data}`, `loaderStyle=global-module`, **no `SDLLaunch.html`**); the host page
> lives in the public monorepo (`packages/frontend/index.html`). The `web/` directory is **gone** —
> the ufront 1.9 cleanup has happened, and with it `shell.html`, the `fetchfs`/`webrtc-net`/`idb-*`
> glue and their Node test suite.

---

## From scratch, in order

Everything below assumes a Debian/Ubuntu host. **Do the prerequisites first** — the build is 32-bit
native and needs an i386 multiarch sysroot that a default install does not have.

### 1. Prerequisites

```bash
# --- compilers: gcc-13 is PINNED by the presets ---------------------------
sudo apt install gcc-13 g++-13 cmake ninja-build git python3
#   Newer GCC is unvalidated against this pre-C++11 tree. CMake must be >= 3.21
#   (tested on 4.2 — GL4ES declares an ancient cmake_minimum_required and the
#   build forces a modern policy floor so it still configures on CMake 4.x).

# --- 32-bit: every native preset is -m32 ----------------------------------
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install gcc-13-multilib g++-13-multilib
sudo apt install libc6-dev:i386 linux-libc-dev:i386 zlib1g-dev:i386 \
                 libx11-dev:i386 libxext-dev:i386 libxrandr-dev:i386 \
                 libxcursor-dev:i386 libxi-dev:i386 libxinerama-dev:i386 \
                 libxss-dev:i386 libxxf86vm-dev:i386 libxkbcommon-dev:i386 \
                 libgl1-mesa-dev:i386 libglu1-mesa-dev:i386 libegl1-mesa-dev:i386 \
                 libgles2-mesa-dev:i386 libdrm-dev:i386 libgbm-dev:i386 \
                 libwayland-dev:i386 libdecor-0-dev:i386 \
                 libasound2-dev:i386 libpulse-dev:i386 libdbus-1-dev:i386 \
                 libibus-1.0-dev:i386 libudev-dev:i386 libopenal-dev:i386
#   SDL2 is built from source by FetchContent and probes for each of these; a
#   missing one does not fail the configure, it silently drops a backend.

# --- Emscripten, for the wasm presets -------------------------------------
git clone https://github.com/emscripten-core/emsdk ~/emsdk
~/emsdk/emsdk install 5.0.7 && ~/emsdk/emsdk activate 5.0.7
#   Validated on 5.0.7. `source ~/emsdk/emsdk_env.sh` before ANY wasm preset —
#   the presets read $EMSDK to find the toolchain file, and a shell without it
#   fails the configure with "toolchainFile not found".

# --- Windows cross-build only (optional) ----------------------------------
sudo apt install g++-mingw-w64-i686-posix binutils-mingw-w64-i686
```

**Network is required on the first configure of each build directory.** SDL2, OpenAL and GL4ES are
not vendored — CMake FetchContent clones them into `build-*/_deps/`. Later builds in the same
directory reuse that and are offline.

**Assets.** Only the wasm link reads them, and only four INIs
(`System/{Default.ini,DefUser.ini,Build.ini,Packages.md5}`) which it `--preload-file`s into MEMFS.
Point at them with `-DUT_ASSETS_DIR=/abs/path` if they are not at `/home/ion/ut2004-assets`. The
native presets read no assets at build time.

> If a wasm configure fails on the preload step, check `ls $UT_ASSETS_DIR` is non-empty first — the
> asset volume is a mount and does not survive a reboot.

### 2. Build

Two commands per target: configure, then build. **Nothing here is incremental across presets** —
each has its own `build-*/` directory and they do not share state.

```bash
# =========================== NATIVE (32-bit, gcc-13) =======================
cmake --preset native
cmake --build --preset native -j"$(nproc)"
#   -> build-native/Source/SDLLaunch/ut2004-pixo.bin

cmake --preset native-karma
cmake --build --preset native-karma -j"$(nproc)"
#   -> build-native-karma/Source/SDLLaunch/ut2004-karma-pixo.bin

# =========================== WEB (needs $EMSDK) ============================
source ~/emsdk/emsdk_env.sh

cmake --preset wasm
cmake --build --preset wasm -j"$(nproc)"
#   -> build-wasm/Source/SDLLaunch/SDLLaunch.{js,wasm,data}

cmake --preset wasm-perf
cmake --build --preset wasm-perf -j"$(nproc)"
#   -> build-wasm-perf/Source/SDLLaunch/SDLLaunch.{js,wasm,data}

# =============== WEB + RECOVERED KARMA (physics on the web) ================
source ~/emsdk/emsdk_env.sh

cmake --preset wasm-karmadecomp-perf
cmake --build --preset wasm-karmadecomp-perf -j"$(nproc)"
#   -> build-wasm-karmadecomp-perf/Source/SDLLaunch/SDLLaunch.{js,wasm,data}
```

**Green checkpoint for this repo:** `native` and `wasm` both build clean.

> **Build trees are disposable and are not kept.** Every `build-*/` directory is gitignored and
> reproducible from the two commands above; they are deleted rather than carried. The **one
> exception** is `build-native-karma/`, which `metoolkit-karma-decomp `decomp/tools/dropin_gap.py`` and
> `reachable.py` read object files from. It has to be that one — an engine built without Karma
> imports no Karma symbols, so those tools would walk an empty closure and report the drop-in gap
> as zero for the wrong reason. Both now refuse that case; see `metoolkit-karma-decomp `decomp/tools/README.md``.

---

## Build matrix — the presets that exist

`cmake --list-presets` is the authority; this table says what each one is *for*. There is no
`gl4es` preset and no `legacy` preset — both were retired in ufront 2.34 (see *Why there is one
native stack* below) and any script still calling them will fail with "no such preset".

| Preset | What it is | Build dir | Output |
|--------|-----------|-----------|--------|
| `native` | 32-bit native. SDL2 + `OpenGLDrv` + `GL4ESDrv` + `PixoDrv`, **all in one binary**; pick at startup. The WASM stack's native twin. | `build-native/` | `Source/SDLLaunch/ut2004-pixo.bin` |
| `native-karma` | Older name for `legacy-karma` below; same configuration, kept so existing commands work. | `build-native-karma/` | `.../ut2004-legacykarma-pixo.bin` |
| **`legacy-karma`** | `native` **+ `BUILD_KARMA_REF`** — links MathEngine's ORIGINAL 32-bit archives instead of the recovered sources. **The A/B reference**, and i386-Linux-only by construction: the shipped members are 32-bit Linux ELF, which is the whole reason `metoolkit-karma-decomp` exists. It earned its keep the day it was added — it played a 300 s ONS match that the recovered build crashed 25 s into. | `build-legacy-karma/` | `.../ut2004-legacykarma-pixo.bin` |
| **`native-nokarma`** | `native` **+ `USE_KARMA_DECOMP=OFF`** — the NO_KARMA control. Not a spare: it is what says whether a failure is Karma's fault or the harness's, and the ut2004 browser suite's 51/51 only meant something because this arm was run through it in the same session. | `build-native-nokarma/` | `.../ut2004-nokarma-pixo.bin` |
| `karma-ref` | The same configuration as `native-karma` in a second directory, kept so the Karma change can be A/B'd without disturbing the 2.34 matrix build. (Its preset description claims "without Pixomatic"; it inherits `native`, which turns Pixomatic on. The description is stale, the config is not.) | `build-karma-ref/` | `.../ut2004-karma-pixo.bin` |
| `physics` | `native` + `WITH_PHYSICS=ON` — the Jolt vehicle sources in `Source/Onslaught`. Not part of the green checkpoint and not exercised recently. | `build-physics/` | `.../ut2004-pixo.bin` |
| `wasm` | **The everyday web build.** `-O2 -g`: named stacks, DWARF, assertions, and still playable. | `build-wasm/` | `Source/SDLLaunch/SDLLaunch.{js,wasm,data}` |
| `wasm-debug` | `-O0`, nothing inlined, every local live. **Last resort** — reach for it when the *optimiser* is the suspect. ~5× the boot time. | `build-wasm-debug/` | same names |
| `wasm-perf` | Shipping: `-O3 -g0`, assertions off. Smallest and fastest. | `build-wasm-perf/` | same names |
| `wasm-nokarma` | `wasm-perf` + `USE_KARMA_DECOMP=OFF` — the NO_KARMA control for the browser suite. | `build-wasm-nokarma/` | same names |
| `wasm-karmadecomp-debug` | **DEPRECATED alias of `wasm-debug`.** Karma is the default now, so the two are the same build. Kept working so existing commands and the handovers do not rot. | `build-wasm-karmadecomp-debug/` | same names |
| `wasm-karmadecomp-perf` | **DEPRECATED alias of `wasm-perf`.** | `build-wasm-karmadecomp-perf/` | same names |
| `windows` | 32-bit Windows cross-build (MinGW-w64), **now with the recovered Karma** — it took three portability fixes nothing else could surface (`proven.txt` `WIN32-PORT-3`). No Pixomatic (its `.lib` is a pure import library and the DLL is not in the drop). ⚠ It BUILDS; it does not RUN under wine, and neither does the `windows-nokarma` control, so that is pre-existing rather than Karma's doing. | `build-windows/` | `Source/SDLLaunch/ut2004.exe` |
| `windows-nokarma` | `windows` + `USE_KARMA_DECOMP=OFF` — the control that established the line above. | `build-windows-nokarma/` | `.../ut2004-nokarma.exe` |
| *(no preset)* **Windows x64** | The 64-bit Windows cross-build, and the LAST target to be exercised at all. Not a preset because it is not a shipping configuration — see the section below for the one-liner. | `build-windows64/` | `Source/SDLLaunch/ut2004.exe` (PE32+ x86-64) |

**The binary name says WHICH PHYSICS it contains**, and since 2026-08-27 that matters more than it
used to: `ut2004`, plus `-legacykarma` for `BUILD_KARMA_REF` or `-nokarma` for neither (the
recovered Karma is the default and so goes unmarked), plus `-pixo` if Pixomatic. Emscripten keeps
the `SDLLaunch` name — the web harness depends on it. Android builds `libUT2004.so`.

> The suffix used to be `-karma` for `BUILD_KARMA_REF` and nothing otherwise. When the recovered
> Karma became the default that made `native` and `native-nokarma` BOTH produce `ut2004-pixo.bin` —
> same filename, opposite physics — and telling those two apart is the entire point of the control.

### Karma is the DEFAULT on every 32-bit target (2026-08-27)

`USE_KARMA_DECOMP` is **ON by default**. `native`, `windows`, `wasm`, `wasm-debug` and `wasm-perf`
all link the recovered Karma; `legacy-karma` asks for MathEngine's originals and the `*-nokarma`
presets ask for neither.

**Android is the exception and it is deliberate.** `Android/app/build.gradle` passes
`-DUSE_KARMA_DECOMP=OFF` explicitly, because two of its three ABIs (`arm64-v8a`, `x86_64`) are LP64
and the recovered sources encode i386 struct LAYOUTS — 128 of 151 structs change size at 64-bit,
silently. The owner's call was to fix LP64 first rather than ship an APK whose physics depends on
which phone you have. Deleting that one line is the whole change once `tools/layout_check.py` is
clean.

CMakeLists enforces this rather than trusting it: `UT_PTR_BYTES` is computed once and a
`USE_KARMA_DECOMP` build on a 64-bit-pointer target is a **hard error**, not a warning.

> ### ⚠ EVERY NATIVE BUILD IS -O0, INCLUDING THE PHYSICS
>
> The optimisation block near the top of this file ends in an `else()` marked "Match Phase 1's -O0
> debug-oriented build for now", and every native configuration lands there — RelWithDebInfo
> included, because the test is `CMAKE_BUILD_TYPE STREQUAL "Debug"`. The flag line reads
> `… -O2 -g -DNDEBUG … -m32 -O0 -g …` and the last `-O` wins, so the engine AND `KarmaDecomp`
> compile unoptimised on Linux and Windows.
>
> That is pre-existing and predates Karma, but it is load-bearing in a way worth knowing: at -O2
> gcc sinks a particular dead null-page load onto the branch that guards it, and at -O0 it does
> not — which is what turned one recovered defect from dormant into a Windows startup crash
> (`proven.txt` `WIN32-PORT-3`). It is also a standing performance question for the native targets.

> ⚠ **`option()` READS AN EXISTING CACHE BEFORE ITS DEFAULT.** A build directory configured before
> the flip keeps NO_KARMA and the only symptom is a game with no vehicles or ragdolls. Every
> configure now prints its physics — `Physics: NO_KARMA ...` or `USE_KARMA_DECOMP -> 145 recovered
> object(s)` — and `rm -rf build-<preset>` is the fix. CI asserts the symbols are actually in the
> binary for the same reason.

> ### ONSLAUGHT RUNS — and the bug that made it not run is worth knowing about
>
> `McdGjkFaceQueueInit` wrote `bits` and `fi[]` — INT fields of `_McdGjkFace` — as FLOATS, because
> Ghidra typed `MePoolxGet`'s result `MeReal *`. `fi[0] = 1` laid down `0x3F800000`, and
> `McdGjkFaceLoad` then walked off the end of the simplex. It faulted ~25 s into ONS-Torlan on
> native i386 AND wasm32, and the ut2004 browser suite — 51/51, almost entirely DM — never touched
> the path. Fixed at the generator (`FLOAT_TYPED_INT_FIELDS`); `proven.txt` `ONS-CRASH-GJKPD`.
>
> **Run an ONS match per build rather than trusting the suite**, which is what
> `metoolkit-karma-decomp `decomp/test/ut2004/ons_smoke.sh` <binary> <label> [seconds]` is for. ONS is the gametype that
> spawns vehicles, and vehicles plus ragdolls are most of what Karma exists for.
> `ONS_RENDERER=-OPENGLRENDERER` for Windows, which has no GLES.
>
> **All three 32-bit targets play Onslaught** as of 2026-08-27: wasm32 330 s in the browser,
> native i386 300 s (plus ONS-Adara and ONS-Ascendancy), Windows 330 s under wine. Android is the
> one target still without Karma, pending the LP64 struct-layout work.
>
> Windows took four defects nothing else could surface, and the last had the widest reach:
> `kd_compat.h` guarded its calling-convention shims with `#ifndef __thiscall`, and **MinGW
> predefines those as real attributes** — so the define-away was skipped and all 140 recovered C++
> methods acquired MSVC's `this`-in-ECX while callers pushed it. Every argument after `this`
> arrived one word early. Zero i386 objects and a byte-identical `.wasm` either way, which is
> exactly why only a Windows build could find it. `proven.txt` `WIN32-PORT-3`.

### The `wasm-karmadecomp-*` presets — what they do differently

**This is the point of the whole `metoolkit-karma-decomp` effort.** MathEngine shipped Karma as binary-only
32-bit x86 archives, so `BUILD_KARMA_REF` can never work off x86 and the web build has always been
`NO_KARMA` — no vehicles, no ragdolls. `metoolkit-karma-decomp` recovered those archives as ordinary
portable C, which compiles for wasm32 like anything else in the tree.

The presets differ from plain `wasm-debug` / `wasm-perf` by **one cache variable**,
`USE_KARMA_DECOMP=ON`, which does two things:

1. **Compile side — identical to `BUILD_KARMA_REF`.** `WITH_KARMA=1` globally (so `NO_KARMA` is
   *not* defined and the eleven `Source/Engine/Src/K*.cpp` bridge files are compiled), plus
   metoolkit's public headers on the include path. Those headers are portable C and were never the
   problem.
2. **Link side — a CMake target instead of a glob of archives.** `Source/Engine/CMakeLists.txt`
   builds a `KarmaDecomp` static library from ``metoolkit_decomp/src/<Lib>/*.c`` (145 recovered
   objects, checked in — see `metoolkit-karma-decomp `metoolkit_decomp/README.md``) plus the hand-written convex-hull
   replacement ``decomp/src/McdConvexCreateHull/kd_convexhull.c``.

`USE_KARMA_DECOMP` and `BUILD_KARMA_REF` are two implementations of the same API and both define
its symbols, so setting both is a hard CMake error.

**There is deliberately no `native-karmadecomp` preset.** On i386 the equivalent already exists and
is stronger: `metoolkit-karma-decomp `decomp/test/standalone/make_dropin_metoolkit.sh`` builds a metoolkit tree with every shipped
member deleted and the engine links and plays against it.

> ✅ **IT RUNS (2026-08-27).** The engine boots in a browser on recovered Karma, loads its
> `KarmaData/*.ka` ragdoll assets and ticks physics. 125 of the 146 archive members contribute at
> least one symbol to the final `.wasm` (3 more define only data, which linking makes unmeasurable
> this way; the remaining 18 are the debug/profiling/XML-output objects and unused constraint types
> `HANDOVER.md` §3b retires) —
> `python3 metoolkit-karma-decomp `decomp/tools/wasm_members.py` build-wasm-karmadecomp-debug` is where those numbers
> come from.
>
> **Getting there took four defects, 49 call sites, all one class, all invisible to every i386 gate**: indirect
> calls whose arity or return type did not match the callee. On i386 cdecl a surplus argument is
> cleaned by the caller and an unwanted result is ignored, so they are inert *by construction*; on
> wasm32 both are part of a function's type and `call_indirect` traps. `proven.txt`
> `WASM-INDIRECT-SIGS` has all three, and `metoolkit-karma-decomp `decomp/HANDOVER-WEB.md`` §0 is the narrative.
>
> **Two gates now cover that class and they see different halves.**
> `python3 metoolkit-karma-decomp `decomp/tools/code_call_check.py` `metoolkit_decomp/src`` reads the SOURCE
> and reports every call still made through the unprototyped `code` type (47 before this work, 0
> now). `python3 metoolkit-karma-decomp `decomp/tools/wasm_indirect_check.py` <build-dir>` reads the linked `.wasm`
> and reports every `call_indirect` whose type no table function has, which is a guaranteed trap
> (0 on both presets). **Neither would have caught every defect** — one was found by reading the
> vendor's struct and one by running the engine — so read both docstrings before quoting a zero.
>
> **Build `wasm-karmadecomp-debug` to diagnose a trap** — its name section gives named frames
> directly in the browser console, and offsets do not transfer between optimisation levels.

Cost, measured on this tree (`.wasm` only; `wasm-perf` vs `wasm-karmadecomp-perf` is like for like —
same `-O3 -g0`, the only difference being Karma):

| | `.wasm` | |
|---|--:|--:|
| `wasm-perf` | 9,872,559 | 9.4 MB |
| `wasm-karmadecomp-perf` | 10,628,369 | 10.1 MB |
| `wasm-karmadecomp-debug` | 38,257,659 | 36.5 MB |

So a complete recovered physics engine costs **756 KB** of shipping wasm, about 7.7%.

---

## `UT_ALLOW_64BIT` — the x86-64 Linux debugging vehicle

**NOT a shipping target.** D-05 stands: the shipped Linux binary is 32-bit. This option exists
for one job — debugging the LP64 defects that only appear at 8-byte pointers.

Android already runs this engine at 64-bit, and with Karma **off** it reaches
`UT2k4MainMenu.Opened()`; with Karma **on** it dies with SIGSEGV. But on Android the engine
installs its own `SA_RESETHAND` fault handler (`Source/Core/Src/UnUnix.cpp`), so there is no
tombstone and no backtrace, and there is no gdb, no ASan and no valgrind. An x86-64 **Linux**
build is the same data model with all of those.

```bash
rm -rf /tmp/kd_b64          # SDL caches its failed X11 probes in SDL_config.h
cmake -S . -B /tmp/kd_b64 -G "Unix Makefiles" \
  -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUT_GFX_BACKEND=gl4es -DUSE_PIXOMATIC=OFF -DUT_ALLOW_64BIT=ON -DUSE_KARMA_DECOMP=ON
cmake --build /tmp/kd_b64 -j 24
file /tmp/kd_b64/Source/SDLLaunch/ut2004.bin    # MUST say ELF 64-bit ... x86-64
```

⚠ **CHECK `file`, NOT THE CONFIGURE MESSAGE.** As first committed this option skipped the
`-m32` demotion block and **four other places put `-m32` straight back** — a bare
`add_compile_options(-m32)`, the exe link options in `SDLLaunch` and `UCC`, and a
`set(CMAKE_C_FLAGS "... -m32" CACHE STRING "" FORCE)` for gl4es that rewrote the flags for the
*whole project*, taking zlib, ogg, vorbis and Speex with it. The build printed
`Pointer width: 8 bytes` and emitted `ELF 32-bit LSB relocatable, Intel i386`. All four are now
gated on `UT_PTR_BYTES`, the variable that already existed to be the width the build actually
produces — but the one-line `file` check is what makes the claim true.

Two things this configuration cannot have:

- **`USE_PIXOMATIC=ON` is impossible at 64-bit** and now fails at configure time with that
  message. `Thirdparty/Pixomatic/Lib/pixomatic_linux.a` is a prebuilt i386 archive with no
  source — the same shape of problem Karma itself was. It used to fail at the last link step,
  after a full 200-TU build.
- **`-lsamplerate` — HANDLED IN-TREE SINCE 2026-09-02, and you should not need to do anything.**
  `samplerate.h` is architecture-INDEPENDENT, so `libsamplerate0-dev:i386` alone satisfies SDL2's
  `check_include_file()`; SDL then appends a bare `-lsamplerate` and the 64-bit link dies with
  `cannot find -lsamplerate`, a message that names nothing and points nowhere.
  `ut_provide_samplerate_linker_name()` in `CMakeLists.txt` now probes with the compiler and, if
  the linker name is missing, provides it inside `${CMAKE_BINARY_DIR}/complib` from the runtime
  SONAME — no root, no apt, nothing outside the build tree. If the runtime library is missing too
  it turns SDL's use of it off and tells you the package.
  ⚠ **This replaced a hand-made symlink in `/tmp/kd_lib64`**, which a reboot wipes; every 64-bit
  build afterwards failed with the message above and nothing connected the two.
  (The amd64 X11 `-dev` packages ARE still needed; without them SDL2's probes fail silently,
  `SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS` goes undefined, and SDL emits a fallback
  `XGenericEventCookie` that collides with the real Xlib one.)

Run it out of a tree that is **not** `ut2004-assets` — a native run writes `UT2004.ini`,
`User.ini` and logs next to itself, and polluting the asset store has cost a session before.
Symlink the content directories in and keep a real, writable `System/`.

---

## Windows **x64** — LLP64, and the three things that are not about pointer width

```bash
cmake -S . -B build-windows64 -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake \
  -DMINGW_TARGET_ARCH=x86_64 -DMINGW_THREAD_MODEL=posix \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUT_GFX_BACKEND=gl4es \
  -DUSE_PIXOMATIC=OFF -DUT_ALLOW_64BIT=ON -DUSE_KARMA_DECOMP=ON
cmake --build build-windows64 -j 12
file build-windows64/Source/SDLLaunch/ut2004.exe   # MUST say PE32+ ... x86-64
```

⚠ **`UT_ALLOW_64BIT=ON` is required even here.** The `-m32` demotion block is gated on
`TARGET_IS_X86 AND NOT MSVC`, and a MinGW x86_64 compiler matches it — without the option the
build hands `-m32` to a toolchain that has no 32-bit sysroot.

★ **MinGW targets LLP64: `long` is FOUR bytes and a pointer is EIGHT.** That is the one data model
this tree had never built for, and all three build failures it caused read as something else:

- **`PTRINT` was `unsigned long`** — four bytes, with eight-byte pointers. And `<windows.h>`'s
  `DWORD` is `unsigned long` too, so PTRINT and DWORD became the **same type** and the build died
  in `UnArc.cpp` with *"redefinition of `operator<<(FArchive&, PTRINT&)`"*. ⚠ The obvious fix —
  `uintptr_t` everywhere — would have **broken web and i386 Linux**, where `uintptr_t` IS
  `unsigned int` IS `DWORD` and the same redefinition appears. Only LLP64 moves
  (`Source/Core/Inc/UnGnuG.h`).
- **`UT_MSVC_NO_MMX`** (`Source/Core/Inc/Core.h`) — three inner loops in `AxWeatherEffect.cpp` and
  `UnParticleEmitter.cpp` were guarded on `_WIN64` when the real question is *"is this MSVC?"*.
  MSVC's x64 compiler dropped the MMX intrinsics; GCC keeps them at every width. MinGW defines
  `_WIN64` **and is GCC**, so it took MSVC's branch and failed on `__m128i::m128i_u32` — an MSVC
  extension — while the `__m64` helper the other branch needs had been `#ifdef`'d away by the same
  test.
- **the crash handler printed 32-bit addresses.** `%08x` truncated the image base, so it reported
  `0x40000000` where the PE's preferred base is `0x140000000`, and every rva it printed symbolised
  to `?? ??:0`. It now prints the base it wants you to add.

**Running it.** `-OPENGLRENDERER` is not optional: Windows has no GLES, and the `-GL4ESRENDERER`
default produces a crash that reads exactly like a physics bug — `proven.txt` records that wrong
renderer producing a confident wrong conclusion in both directions at once.

```bash
WINEPREFIX=~/.wine64ut wine64 ./ut2004.exe \
  "ONS-Torlan?game=Onslaught.ONSOnslaughtGame?SpectatorOnly=1?NumBots=6?QuickStart=True" \
  -OPENGLRENDERER -nohomedir -FIXEDFPS=30 -KTRACE=ktrace-win64.csv
```

★ **Measured 2026-09-02: 11663 physics frames — 388 s of simulated time — 0 faults**, with Manta,
Hover Bike, Goliath, PRV and RV all moving. ⚠ **Frame advance is verified in the TRACE, not the
log**: this engine's HUD ticks on the render path, outside `ULevel::Tick`'s `IsPaused()` guard, so
log output keeps flowing through a paused level. The first run of this build crashed at 2651
frames in `McdConvexMeshPlaneCut` reading `0xffffffff` — a Karma LP64 defect, now repaired in the
recovery's generator (karma-decomp `proven.txt` `LP64-GJK-CONTACT`).

---

## `UT_NO_FAULT_HANDLER` — let the OS see the fault

**A diagnostic, not a build option.** It is read at run time and unset behaviour is exactly what it
always was, so nothing about a shipping build changes.

The engine installs its own `SA_RESETHAND` handler for the six hardware-fault signals
(`Source/Core/Src/UnUnix.cpp`, `appPlatformInit`). ★ **That handler is Epic's** — it is present
verbatim in `epic-sources/ut99-v400/Core/Src/UnUnix.cpp`; only the switch below is ours. It catches
`SIGSEGV`, writes one line to the log and exits. On Linux that is harmless because gdb gets in
first. **On Android nothing gets in first**: `debuggerd` never runs, so there is no tombstone and no
backtrace — a Karma crash reads as `Signal: SIGSEGV` and a clean shutdown, and says nothing about
where. That is why Karma-on-Android had no backtrace at all until 2026-08-31.

With the switch on, `SIGSEGV SIGILL SIGTRAP SIGIOT SIGBUS SIGFPE` are left at their default
disposition and the platform's crash reporter produces a real one. The job-control signals are
installed either way — those are how the engine shuts down cleanly, and dropping them would turn a
clean exit into a kill.

| | |
|---|---|
| everywhere but Android | `UT_NO_FAULT_HANDLER=1 ./ut2004.bin ...` (any non-empty value) |
| Android | `adb shell setprop debug.ut2004.nofault 1` |

⚠ **Android has no way to set an environment variable for an app.** `adb shell` is a different
process and the launcher is Java, so the variable that works everywhere else is unreachable exactly
where it was needed. A `debug.*` system property is the idiomatic equivalent and is settable on a
stock user build without root. The property is read once, in `appPlatformInit`, so set it **before**
launching; it takes effect on the next start.

Confirm it took, then read the tombstone:

```bash
adb shell setprop debug.ut2004.nofault 1
adb shell monkey -p com.ut2004.android -c android.intent.category.LAUNCHER 1
adb logcat -c && adb logcat -s DEBUG               # the tombstone lands here
# frame addresses -> source, against the UNSTRIPPED .so:
llvm-addr2line -f -C -e Android/app/build/intermediates/cxx/Debug/*/obj/arm64-v8a/libUT2004.so <addr>
```

The log line `UT_NO_FAULT_HANDLER set: leaving SIGSEGV/... to the OS.` in `UT2004.log` is the
confirmation that the switch was seen — check for it rather than assuming the property took.

⚠ `am start -n com.ut2004.android/.GameActivity` **fails**: that activity is not exported. Use
`monkey` as above, or `am start -n com.ut2004.android/.MainActivity`, which is.

### Starting a map from `adb` (ufront 2.45)

`MainActivity` forwards a `com.ut2004.android.ARGS` string extra to the game process, which appends
it to the engine command line after `CPUSPEED=3000`. It is the Android equivalent of
`ut2004 ONS-Torlan?Game=...` on the desktop; before it existed the activity could only ever launch
the menu, so "does arm64 *play*?" could not be answered in a scripted run.

```bash
adb shell am start -n com.ut2004.android/.MainActivity \
  --es com.ut2004.android.ARGS \
  'ONS-Dria?Game=Onslaught.ONSOnslaughtGame?bAutoNumBots=True?QuickStart=True?bPlayerMustBeReady=False'
```

The launcher's 10-second auto-start countdown then needs no touch, so the whole run is one command.
`?bAutoNumBots=True?QuickStart=True?bPlayerMustBeReady=False` is what makes the match **tick**: a URL
carrying only `?Game=` loads the level, burns CPU and never leaves the pre-match state.

---

## Why there is one native stack (ufront 2.34)

BUILD.md used to describe a "hard backend split" between a `legacy` preset (SDL 1.2 + `OpenGLDrv`)
and a `gl4es` preset (SDL 2 + `GL4ESDrv`), each locked to its own build directory. **Both presets
are gone and so is the split.** SDL 1.2 is retired: `SDLDrv` and `Thirdparty/SDL12` no longer exist,
and asking for it (`-DUSE_SDL12=ON`, `-DUT_GFX_BACKEND=` anything but `gl4es`) is now a
`FATAL_ERROR` rather than a silently different build.

The two halves of the old argument:

- *"SDL 1.2 and SDL 2 export clashing symbols"* — true, and moot; there is only SDL2 now.
- *"`OpenGLDrv` and `GL4ESDrv` cannot coexist in one translation unit"* — literally true (each
  pulls a different `GL/gl.h`) and **already contained**: both set their include directories
  `PRIVATE`, so neither ever sees the other's header, and they are never in one TU. Their symbols
  do not collide either — `OpenGLDrv` links no GL library at all (it binds through a pointer table
  filled by `SDL_GL_GetProcAddress`) and gl4es compiles `-fvisibility=hidden` off-Windows.

So `native` carries **both** GL render devices plus Pixomatic in one binary and the choice moves to
startup. `UT_GFX_BACKEND` survives only so existing build directories and the Android/web branches
have a value to read; `gl4es` is the only accepted setting.

## Render-device selection (`-*RENDERER` switches)

`SDLLaunch.cpp`'s `SelectRenderDevice()` (ufront 2.9) picks the render device from a single
command-line switch, native **and** web. Each switch names a render device and (for the two GL
drivers, coupled 1:1 to an SDL viewport manager) its matching `ViewportManager`:

| Switch | Render device | Viewport | Available in |
|--------|---------------|----------|--------------|
| `-GL4ESRENDERER`   | `GL4ESDrv.GL4ESRenderDevice`   | `SDL2Drv.SDL2Client` | all presets (the default on web) |
| `-OPENGLRENDERER`  | `OpenGLDrv.OpenGLRenderDevice` | `SDL2Drv.SDL2Client` | `native*`, `karma-ref`, `physics`, `windows` |
| `-SOFTWARERENDERER`| `PixoDrv.PixoRenderDevice`     | (keep current)       | `native*`, `karma-ref`, `physics` (`USE_PIXOMATIC=ON`, never on web or Windows) |
| `-NULLRENDERER`    | `Engine.NullRenderDevice`      | (keep current)       | all presets |

- Availability is a `StaticFindObject` on the class' short name — a device statically linked into
  this preset registered its `UClass` at startup. A switch for a driver **not** compiled into the
  current preset logs `render device X not available` and **no-ops** — the game still boots on the
  default (the canary). This also fixed the old latent `-SOFTWARE`→`PixoDrv` fatal (it set an
  uninstantiable device, then aborted).
- With **no** switch, the render device is left to the shipped ini + the existing `InitEngine()`
  auto-switch guards (`WinDrv`→SDL, `D3D`→OpenGL, `OpenGL`/`GLES3`→`GL4ES` on SDL2) — those guards
  already normalize the persisted/default choice to the built stack, so `SelectRenderDevice()`
  doesn't duplicate or fight them (the no-switch boot is unchanged).
- A switch that survives `Engine->Init()` is persisted to the ini (`PersistRenderChoiceIfSwitched()`
  → `GConfig->Flush`) so the pick sticks next boot (on the web, via the user-data IDB layer). The
  frontend's per-target `videoDrivers` DATA maps a friendly label → the switch.

## Repository layout

| Dir | What |
|-----|------|
| `Source/` | all engine C++ — Core, Engine, Fire, IpDrv, XGame, XInterface, UWeb, Onslaught, ALAudio, the render/viewport drivers, the `SDLLaunch`/`UCC` launchers, plus the kept Windows/editor/tool code. The code the build compiles. |
| `Thirdparty/` | bundled deps: `zlib`, `Vorbis`, `Speex`, `OpenAL` (headers), `metoolkit` (Karma), `Pixomatic`. SDL2 + GL4ES are FetchContent'd, not vendored. |
| `Engine/Config/` | tracked config: `Default.ini`/`DefUser.ini` = platform-agnostic / native reference; **`web/`** = the web platform set, **build-consumed** (the wasm link `--preload-file`s it). See `Engine/Config/README.md`. |
| *(not here)* | the Karma decompilation moved OUT in ufront 2.44 — sources, generator, gates and handovers are [`10-p/metoolkit-karma-decomp`](https://github.com/10-p/metoolkit-karma-decomp), fetched at configure time. |
| `Android/` | the NDK/Gradle project; AGP wraps the root `CMakeLists.txt`. |
| `vendor/glue/` | the monorepo ABI contract baked into the web build (`ue_abi.h`, `webrtc-shim.js`). |
| `cmake/`, `docs/` | CMake modules; this repo's docs. |

Build config is the root `CMakeLists.txt` + `CMakePresets.json`.

## Android (native NDK build — x86_64 first; ufront 2.10a)

A separate **native NDK/Gradle** build wrapping the engine as `libUT2004.so`. It reuses the SDL2 +
`GL4ESDrv` stack (→ native EGL GLES2) — **not** wasm. **The web build is priority #1 and stays
byte-identical:** every Android delta is behind `#if defined(__ANDROID__)` (C++) / `if(ANDROID)`
(CMake), which only the NDK toolchain sets — `emcmake` and the gcc-13 presets never do.

- **Where:** `Android/` (Gradle 8.7 / AGP 8.5.2; `GameActivity extends SDLActivity`; vendored SDL
  **2.32.11** Java layer, version-matched to the engine's `release-2.32.x` SDL FetchContent).
- **Integration (Option B):** AGP's `externalNativeBuild.cmake.path` points at THIS repo's root
  `CMakeLists.txt` (`targets SDLLaunch`); the `if(ANDROID)` branches build `SDLLaunch` as a SHARED
  `libUT2004.so` (whole-archive, same package set as the native preset). No wrapper CMake — keeps
  `CMAKE_SOURCE_DIR` = repo root, so the existing `${CMAKE_SOURCE_DIR}` refs + the single
  FetchContent hold.
- **Android divergences from the `native` preset:** 64-bit (drop `-m32` / `-DPLATFORM_32BIT` /
  x86 `-march`); clang leniency instead of gcc `-fpermissive`; the **ANSI `NO_UNICODE_OS_SUPPORT`
  path (like wasm)**, NOT `-fshort-wchar`/UNICODE — bionic's 32-bit `wchar_t` + non-glibc
  `<wchar.h>` guard clash with the engine's 16-bit `wcs*` redeclarations; SDL2 built SHARED
  (`libSDL2.so`, aliased to `SDL2::SDL2-static`); stdout/stderr → logcat (`UT2004` tag; SELinux
  blocks `setprop log.redirect-stdio`). There is **no** `-fpack-struct=4` here (that was the
  UE1/UTA build) — packing stays `#pragma pack` under `-DSUPPORTS_PRAGMA_PACK=1`.
- **★ 64-bit AND KARMA NOW MIX, AND KARMA IS THE DEFAULT (2026-09-01).** `USE_KARMA_DECOMP` used
  to be a 32-bit-pointer assumption — the recovered sources encoded i386 struct layouts — and this
  line used to say *"armv7 is fine; arm64 is not"*. The LP64 post-passes in
  `metoolkit-karma-decomp` closed that. Measured on a OnePlus 6, `ONS-Dria` Onslaught with bots,
  vehicles and ragdolls, run as a **spectator** so nothing can pause the level:

  | ABI | result |
  |---|---|
  | `arm64-v8a` | **1397 s (23 min), 0 signals**, frame advance verified every 60 s |
  | `armeabi-v7a` | the same match on the 32-bit slice: **508 s, 0 signals** |

  ★ The armv7 arm is the attribution control: **both widths play**, so arm64 is not passing because
  64-bit is lenient. `./gradlew :app:assembleDebug` now builds a Karma APK; **`-PnoKarma`** builds
  the old physics-free control. ⚠ `x86_64` is in the APK and has still never been run.
  ⚠ **Verify in the artefact, not the gradle log** — it says "BUILD SUCCESSFUL, up-to-date" whether
  or not it recompiled. `llvm-objdump -d --disassemble-symbols=MePoolxDictNodeAllocate <arm64 .so>`
  should show `mov x9, #0x10` / `#0x14` (the LP64 `MePoolx` offsets); `0xc`/`0x10` are the i386
  ones and mean the object is stale.

```bash
# NDK r30 + SDK cmake + Android Studio JBR 21. Emulator: x86_64 / API-34 (arm64 runs via translation).
cd Android
JAVA_HOME=/home/ion/android-studio/jbr ./gradlew :app:assembleDebug   # -> app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.ut2004.android/.GameActivity                # SDL_main runs the engine; logcat tag UT2004
```

- **Status (2.10a — build standup):** builds + links + installs + launches; `SDL_main` runs the
  engine into `appInit`, which stops at `MissingIni` (no game data yet — external-storage access is
  **2.10b**). arm64-v8a, file management, gamepad, touch, net, and lifecycle follow — see the
  monorepo `docs/migration/steps/2.10-android.md`.

## Assets (`UT_ASSETS_DIR`)

The WASM link **preloads 4 INIs** into MEMFS (needed during `appInit`):
`System/{Default.ini,DefUser.ini,Build.ini,Packages.md5}`. They are read from `$UT_ASSETS_DIR`:

```bash
cmake --preset wasm -DUT_ASSETS_DIR=/abs/path/to/ut2004-assets
```

Default: `/home/ion/ut2004-assets`. The native presets do **not** read assets at build time (the
native binary only compiles here; running it needs a runtime dir with INIs + assets, out of scope
for the build checkpoint). All other game content is streamed at runtime by FetchFS over HTTP from
the deployed asset tree — it is never a build input.

## GL4ES source: fork by default, switchable to upstream

The native and wasm builds fetch GL4ES (via CMake FetchContent) from our **patched fork by
default**, because stock upstream `v1.1.6` renders environment-mapped (cubemap) surfaces wrong. The
fork fixes the active-unit cube bind and moves reflection-map texgen to the fragment shader. The
companion driver-side fixes live in `GL4ESDrv/`.

```bash
# Default — fork (nothing to pass):
cmake --preset native
# Stock upstream (broken cubemaps; A/B only):
cmake --preset native -DGL4ES_GIT_REPOSITORY=https://github.com/ptitSeb/gl4es.git -DGL4ES_GIT_TAG=v1.1.6
# Local checkout (GL4ES dev):
cmake --preset native -DFETCHCONTENT_SOURCE_DIR_GL4ES=/abs/path/to/gl4es
```

- Default source: `https://github.com/10-p/gl4es`, branch `fix/cubemap-bind-active-unit`.
- The fork choice is interface-neutral — upstreaming later is just a default flip. Called out
  explicitly so it is never silently lost.

> The WASM build also FetchContent-clones GL4ES (and builds SDL2/OpenAL ports), so the **first**
> configure of a fresh build dir needs network access. Subsequent builds reuse `build-*/_deps/`.

## WASM: the three modes

Two knobs, both set by the presets: `UT_WEB_PERF` and `UT_WEB_DEBUG`.

| preset | `UT_WEB_PERF` | `UT_WEB_DEBUG` | compile | for |
|---|---|---|---|---|
| `wasm-perf` | ON | – | `-O3 -g0` | shipping: fastest, smallest, no symbols |
| `wasm` | OFF | OFF | `-O2 -g` | **the dev default**: named stacks, DWARF, assertions |
| `wasm-debug` | OFF | ON | `-O0 -g` | last resort: every local live, nothing inlined |

They also gate the Emscripten link flags (`-g -gsource-map -sASSERTIONS=2
-sSTACK_OVERFLOW_CHECK=2` ↔ `-O3 -sASSERTIONS=0`). Everything else is shared: Asyncify, SDL2/OpenAL
ports, `FULL_ES2`, memory growth, the baked FetchFS/IDB/WebRTC JS libraries, the 16 MB
temp-vertex-buffer-cap post-link fix.

**⚠ The dev build is not the small one.** Measured 2026-08-03:

| mode | total | DWARF | code |
|---|--:|--:|--:|
| `wasm-perf` | 9.4 MB | 0.0 | 8.9 |
| `wasm-debug` | 32.8 MB | 21.7 | 9.8 |
| `wasm` | 49.0 MB | 27.4 | 20.6 |

`-O2`'s code section is more than twice `-O0`'s, because DWARF forces Emscripten to run only
limited binaryen optimizations and these builds link at `-O1`, so `-O2`'s inlining is never cleaned
up the way the perf build's is. Speed is what the dev default buys, not size.

Deliberately **not** used (to keep the perf build safe): **Closure** (would mangle the hand-written
JS glue baked via `--js-library`) and **LTO** (risks eliminating the engine's
`AUTO_INITIALIZE_REGISTRANTS_*` global constructors that `--whole-archive` preserves). `-g0` in the
perf path is load-bearing — *any* DWARF forces binaryen into limited-optimization mode and bloats
the wasm.

## Vendored third-party (`Thirdparty/`)

Bundled dependencies live under `Thirdparty/`: `zlib`, `Vorbis` (ogg+vorbis), `Speex`, `OpenAL`
(headers), and the closed prebuilt libs `metoolkit/` (Karma physics) + `Pixomatic/` (software
renderer). SDL2 and GL4ES are **not** vendored — they're fetched via CMake FetchContent.

Karma has three states, not two: **off** (the default, `NO_KARMA=1`), **`BUILD_KARMA_REF=ON`**
(MathEngine's prebuilt 32-bit x86 archives; the `native-karma`/`karma-ref` presets), and
**`USE_KARMA_DECOMP=ON`** (the recovered sources; the `wasm-karmadecomp-*` presets). Pixomatic is
`USE_PIXOMATIC` — on for every native preset, never on web or Windows.

The former non-game bloat has been **removed** (see git history): the bundled `DirectX8/9/9_AMD64`
SDKs and the `Microsoft Platform SDK for Windows XP SP2` (the kept `WinDrv`/`D3DDrv` code now uses
system `<netfw.h>`/`<d3d8.h>` includes for a future Windows port), retail installers/tools
(`Help/`, `Manual/`, `Tools/`, `RemoteDebugging/`, `unxutils/`), the dead pre-CMake MSVC/makefile
build, and the retail `System/` runtime dir + `Content/` game-content archive (both redundant with
the asset tree). Nothing here is a build input beyond `Source/` + `Thirdparty/` (+ the fetched recovery
for `USE_KARMA_DECOMP`); the engine builds/runs from prebuilt `.u` + assets in `$UT_ASSETS_DIR`.

## Verifying a build did what you asked

```bash
# Both GL devices are in the native binary, and the software one too:
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UOpenGLRenderDevice   # > 0
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UGL4ESRenderDevice    # > 0
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UPixoRenderDevice     # > 0

# The recovered Karma really is IN the .wasm, not merely built beside it. A
# static archive contributes only the members something references, so "145
# recovered object(s)" at configure time proves nothing about the binary.
python3 metoolkit-karma-decomp `decomp/tools/wasm_members.py` build-wasm-karmadecomp-debug
#   -> 125 contribute at least one symbol / 3 data-only / 18 not referenced,
#      and those 18 are the profiling, debug, XML-output and unused-constraint
#      objects HANDOVER.md 3b retires. Needs a build with a name section: the
#      tool REFUSES a -g0 (*-perf) tree rather than reporting it as empty.
```

## Tests

**This repo has no test suite of its own for the web artifact, and should not grow one.** It
produces `SDLLaunch.{js,wasm,data}`; everything that exercises that artifact in a browser — the
host page, the glue, the end-to-end suite — belongs to the public monorepo and is run there. The
former in-repo `web/` directory and its Node tests are gone.

The gates for the Karma decompilation are NOT here any more (ufront 2.44) — they are the fetched
repository's `decomp/test/`, split into a standalone tier and one that needs this game. They test recovered
C against MathEngine's shipped library, not the engine or the browser. `metoolkit-karma-decomp `decomp/HANDOVER.md``
its `decomp/HANDOVER.md` §4 lists them and `decomp/test/README.md` documents each one.
