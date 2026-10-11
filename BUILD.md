# Building engine-ut2004 (native + WebAssembly)

This is the **private engine repo** for the UT2004 (UE2.5) web port — engine source only. Game
assets (~5.8 GB: maps, textures, meshes, sounds, animations, compiled `.u` packages) live **out of
tree** and are never tracked here; only the wasm link reads two files from them (`-DUT_ASSETS_DIR`).

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

The package names below are Debian/Ubuntu; [README.md](README.md) has the Arch equivalents. **Do the prerequisites first** — the build is 32-bit
native and needs an i386 multiarch sysroot that a default install does not have.

### 1. Prerequisites

```bash
# --- compilers: the system gcc (tested: 13 in CI, 15.2, 16.2) ---------------
sudo apt install gcc g++ cmake ninja-build git python3
#   CMake must be >= 3.21
#   (tested on 4.2 — GL4ES declares an ancient cmake_minimum_required and the
#   build forces a modern policy floor so it still configures on CMake 4.x).

# --- 32-bit: every native preset is -m32 ----------------------------------
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install gcc-multilib g++-multilib
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
git clone https://github.com/emscripten-core/emsdk <emsdk>
<emsdk>/emsdk install 5.0.7 && <emsdk>/emsdk activate 5.0.7
#   Validated on 5.0.7. `source <emsdk>/emsdk_env.sh` before ANY wasm preset —
#   the presets read $EMSDK to find the toolchain file, and a shell without it
#   fails the configure with "toolchainFile not found".

# --- Windows cross-build only (optional) ----------------------------------
sudo apt install g++-mingw-w64-i686-posix binutils-mingw-w64-i686
```

**Network is required on the first configure of each build directory.** SDL2, OpenAL and GL4ES are
not vendored — CMake FetchContent clones them into `build-*/_deps/`. Later builds in the same
directory reuse that and are offline.

**Assets.** Only the wasm link reads them, and only `System/{Build.ini,Packages.md5}` (the two boot INIs come
from `Engine/Config/web`), which it `--preload-file`s into MEMFS. Point at the game folder with
`-DUT_ASSETS_DIR=/abs/path`; the configure stops early if the two files are not there. The native presets
read no assets at build time.

> If a wasm configure fails on the preload step, check `ls $UT_ASSETS_DIR` is non-empty first — the
> asset volume is a mount and does not survive a reboot.

### 2. Build

Two commands per target: configure, then build. **Nothing here is incremental across presets** —
each has its own `build-*/` directory and they do not share state.

```bash
# =========================== NATIVE (32-bit) ================================
cmake --preset native
cmake --build --preset native -j"$(nproc)"
#   -> build-native/Source/SDLLaunch/ut2004-pixo.bin

cmake --preset windows64
cmake --build --preset windows64 -j"$(nproc)"
#   -> build-windows64/Source/SDLLaunch/ut2004.exe   (run it under wine; see ktrace_gate.sh)

# =========================== WEB (needs $EMSDK) ============================
source <emsdk>/emsdk_env.sh

cmake --preset wasm
cmake --build --preset wasm -j"$(nproc)"
#   -> build-wasm/Source/SDLLaunch/SDLLaunch.{js,wasm,data}

cmake --preset wasm-perf
cmake --build --preset wasm-perf -j"$(nproc)"
#   -> build-wasm-perf/Source/SDLLaunch/SDLLaunch.{js,wasm,data}

# =============== WEB + RECOVERED KARMA (physics on the web) ================
source <emsdk>/emsdk_env.sh

cmake --preset wasm-karmadecomp-perf
cmake --build --preset wasm-karmadecomp-perf -j"$(nproc)"
#   -> build-wasm-karmadecomp-perf/Source/SDLLaunch/SDLLaunch.{js,wasm,data}
```

**Green checkpoint for this repo:** `native` and `wasm` both build clean.

> **Build trees are disposable and are not kept.** Every `build-*/` directory is gitignored and
> reproducible from the two commands above; they are deleted rather than carried. The **one
> exception** is a Karma build — `build-native/` will do — which `metoolkit-karma-decomp
> `decomp/tools/dropin_gap.py`` and `reachable.py` read object files from. It has to be a Karma one:
> an engine built without Karma
> imports no Karma symbols, so those tools would walk an empty closure and report the drop-in gap
> as zero for the wrong reason. Both now refuse that case; see `metoolkit-karma-decomp `decomp/tools/README.md``.

---

## Build matrix — the presets that exist

`cmake --list-presets` is the authority; this table says what each one is *for*. There is no
`gl4es` preset and no `legacy` preset — both were retired in ufront 2.34 (see *Why there is one
native stack* below) and any script still calling them will fail with "no such preset".

| Preset | What it is | Build dir | Output |
|--------|-----------|-----------|--------|
| `native` | **32-bit native, and still the DEFAULT.** SDL2 + `OpenGLDrv` + `GL4ESDrv` + `PixoDrv`, **all in one binary**; pick at startup. The WASM stack's native twin. ★ It is also the pointer-width **CONTROL arm** of `decomp/test/ut2004/ktrace_gate.sh`, which compares a 64-bit candidate against it byte-for-byte — which is why the default did not move to 64-bit. | `build-native/` | `Source/SDLLaunch/ut2004-pixo.bin` |
| `native-sse` | **The ktrace CONTROL arm (ufront 2.58).** 32-bit with `-msse2 -mfpmath=sse` (`UT_FPMATH_SSE=ON`), no Pixomatic: the same recovered Karma at 32-bit pointer width and SSE float math, so the only variable against `native64` is the pointer width. `native` cannot be that control (x87, `81b06b0cf715`). | `build-native-sse/` | `Source/SDLLaunch/ut2004.bin` |
| `native64` | **64-bit native (ufront 2.54).** Same engine and recovered Karma at LP64. ⚠ **No Pixomatic** — the prebuilts are i386-only on *both* platforms and there is no source, so `-SOFTWARERENDERER` does not exist here; gl4es (built from source) is the fallback. Configure reports `Pointer width: 8 bytes` and an explicit LP64-Karma note. | `build-native64/` | `Source/SDLLaunch/ut2004.bin` |

> ⚠ **A build dir created BEFORE the 2.44 Karma relocation will not configure**, and the message points
> at a path that no longer exists:
>
> ```
> metoolkit_decomp: no Karma headers at .../engine-ut2004/Thirdparty/metoolkit/include.
> ```
>
> `METOOLKIT_DIR` is a `CACHE PATH`, so the fetched Karma repo's own default cannot overwrite the stale
> value baked into `CMakeCache.txt` back when the SDK lived in `Thirdparty/`. It is not a source problem
> and there is nothing to rebuild from scratch — clear the one entry:
>
> ```bash
> cmake -U METOOLKIT_DIR -S . -B build-wasm     # then build the preset as usual
> ```
>
> Measured 2026-09-03: `build-wasm` failed this way while `build-wasm-perf` built fine, purely because
> the latter had been configured after the move. Any dir can be checked with
> `grep ^METOOLKIT_DIR <dir>/CMakeCache.txt` — it should name `_deps/metoolkitkarmadecomp-src/…`.
| **`native-nokarma`** | `native` **+ `USE_KARMA_DECOMP=OFF`** — the NO_KARMA control. Not a spare: it is what says whether a failure is Karma's fault or the harness's, and the ut2004 browser suite's 51/51 only meant something because this arm was run through it in the same session. | `build-native-nokarma/` | `.../ut2004-nokarma-pixo.bin` |
| **`legacy-karma`** | `native` **+ `BUILD_KARMA_REF=ON`, `USE_KARMA_DECOMP=OFF`** — the **A/B REFERENCE ARM** (restored 2026-09-12). Identical engine and renderers; MathEngine's own prebuilt i386 archives as the physics instead of the recovered sources. This is what the recovery is judged against, and it replaced `native-nokarma` in CI: comparing against the real thing answers *is the recovered code right*, where comparing against no physics only answered *is Karma involved*. **32-bit x86 only** — the archives are binary-only i386 and the build refuses early rather than fail at the link. ✅ Measured 2026-09-12: 40/40 frames on `test-karma-1` and `ktrace_score.py` says `MATCH` (8/8 bodies at rest, 8/8 asleep) against `native`. | `build-legacy-karma/` | `.../ut2004-legacykarma-pixo.bin` |
| `physics` | `native` + `WITH_PHYSICS=ON` — the Jolt vehicle sources in `Source/Onslaught`. Not part of the green checkpoint and not exercised recently. | `build-physics/` | `.../ut2004-pixo.bin` |
| `wasm` | **The everyday web build.** `-O2 -g`: named stacks, DWARF, assertions, and still playable. | `build-wasm/` | `Source/SDLLaunch/SDLLaunch.{js,wasm,data}` |
| `wasm-debug` | `-O0`, nothing inlined, every local live. **Last resort** — reach for it when the *optimiser* is the suspect. ~5× the boot time. | `build-wasm-debug/` | same names |
| `wasm-perf` | Shipping: `-O3 -g0`, assertions off. Smallest and fastest. | `build-wasm-perf/` | same names |
| `wasm-nokarma` | `wasm-perf` + `USE_KARMA_DECOMP=OFF` — the NO_KARMA control for the browser suite. | `build-wasm-nokarma/` | same names |
| `wasm-karmadecomp-debug` | **DEPRECATED alias of `wasm-debug`.** Karma is the default now, so the two are the same build. Kept working so existing commands and the handovers do not rot. | `build-wasm-karmadecomp-debug/` | same names |
| `wasm-karmadecomp-perf` | **DEPRECATED alias of `wasm-perf`.** | `build-wasm-karmadecomp-perf/` | same names |
| `windows` | 32-bit Windows cross-build (MinGW-w64), **now with the recovered Karma** — it took three portability fixes nothing else could surface (`proven.txt` `WIN32-PORT-3`). ★ **ufront 2.54: Pixomatic is ON here.** The earlier note that the DLL "is not in the drop" was half right — it was absent from `Thirdparty/`, but it ships in a retail `System/`, and it is now **vendored** at `Thirdparty/Pixomatic/Bin/pixomatic.dll` (PE32 i386, byte-identical to retail) and staged beside the exe. ⚠ `pixomatic.lib` is a pure IMPORT library and nothing is delay-loaded, so the DLL is a **load-time** dependency: without it the process does not start at all — measured, `objdump -p` lists `pixomatic.dll` and the Delay Import Directory is empty. It runs under wine to `UT2K4MainMenu` (`-SOFTWARERENDERER`, Xvfb; see the note below on the wine loader). | `build-windows/` | `Source/SDLLaunch/ut2004-pixo.exe` + `pixomatic.dll` |
| **`windows64`** | 64-bit Windows (MinGW-w64 cross) **with the recovered Karma** — the only reason an x64 Windows build exists, since MinGW cannot link MathEngine's MSVC-mangled prebuilts. **LLP64**: `long` is 4 bytes where Linux's is 8 and pointers are 8 on both, so it is the only target exercising that combination. Validated through the same trajectory gate as everything else (`ktrace_gate.sh` runs a `.exe` under wine). | `build-windows64/` | `.../ut2004.exe` |
| `windows-nokarma` | `windows` + `USE_KARMA_DECOMP=OFF` — the control that established the line above. | `build-windows-nokarma/` | `.../ut2004-nokarma.exe` |
| *(no preset)* **Windows x64** | The 64-bit Windows cross-build, and the LAST target to be exercised at all. Not a preset because it is not a shipping configuration — see the section below for the one-liner. | `build-windows64/` | `Source/SDLLaunch/ut2004.exe` (PE32+ x86-64) |

### `meta.json` records what was fetched (ufront 2.70)

`toolchain` names every repository FetchContent pulled for the web artifact, as `<repo>@<branch|tag>` —
never a commit: `gl4es` (`10-p/gl4es@fix/cubemap-bind-active-unit`), `karma`
(`10-p/metoolkit-karma-decomp@main`) and `openal-soft` (`10-p/openal-soft-web@ufront/web`) are consumed at
their branch tip so a rebuild takes the latest of each ("everything is made for this project"). The values
come from the same cache variables the `FetchContent_Declare`s read (`cmake/ArtifactDeps.cmake`:
`GL4ES_GIT_*`, `KARMA_DECOMP_GIT_*`, `OPENALSOFT_GIT_*`), so the descriptor cannot drift from the fetch; a
local checkout override (`FETCHCONTENT_SOURCE_DIR_<NAME>`) shows up as `local:<path>` and marks the artifact
as not reproducible from its descriptor. `emscripten` is read from the toolchain's own version file (it used
to say `unknown` because `emcc` is not on PATH in a preset build). ⚠ A branch ref is re-fetched on every
**configure** (`cmake --preset …`), never by a plain `cmake --build`: an old build directory rebuilt without
re-configuring still holds the dependency it fetched last time; `publish-all` always configures first.

### openal-soft on Android and native Linux (ufront 2.61 / 2.70)

One fork everywhere: `10-p/openal-soft-web` `ufront/web` (openal-soft 1.25.2 + the Web Audio backend + the
wasm build patches). The web links it static; Android (2.61, converged from an upstream 1.23.1 pin in 2.70)
and native Linux (2.70) build it SHARED with `ALSOFT_EAX=ON` — the root `CMakeLists.txt` block declares the
`OPENALSOFT_GIT_*` cache variables once for all three. ALAudio **dlopens** `libopenal.so` at audio-init and
the library is never DT_NEEDED (the engine exports its global `operator new` and `GMalloc` does not exist at
process start; a load-time openal crashed in its static initialisers on Android). On Linux, SDLLaunch's
POST_BUILD copies it beside the binary as `libopenal.so`, found through the executable's rpath `.` from the
`System/` cwd: **a run tree that takes the binary must take `libopenal.so` with it**, or the game silently
falls back to the distro's `libopenal` (Ubuntu's has no EAX → `ALAudio: EAX off`). Backends are whatever the
host's headers allowed at configure time, each dlopen'd at run time; the configure log says which. ⚠ On this
box the 64-bit `native64` tree gets only `OSS, Null` — the ALSA/PulseAudio dev packages are installed for
i386 only — so the 64-bit binary opens no device (`ALAudio: no OpenAL devices found`); `libasound2-dev:amd64`
fixes that, the 32-bit shipping build gets `PulseAudio, ALSA, OSS, Null`. `-DUT_BUNDLED_OPENAL=OFF` restores
the old behaviour (dlopen of the system library).

**The binary name says WHICH PHYSICS it contains**: `ut2004`, plus `-legacykarma` when
`BUILD_KARMA_REF=ON` or `-nokarma` when `USE_KARMA_DECOMP=OFF` (the recovered Karma is the default
and so goes unmarked), plus `-pixo` if Pixomatic. Emscripten keeps
the `SDLLaunch` name — the web harness depends on it. Android builds `libUT2004.so`.

> The suffix used to be `-karma` for the legacy archives and nothing otherwise. When the recovered
> Karma became the default that made `native` and `native-nokarma` BOTH produce `ut2004-pixo.bin` —
> same filename, opposite physics — and telling those two apart is the entire point of the control.
> (`-legacykarma` went with `BUILD_KARMA_REF` on 2026-09-02 and came back with it on 2026-09-12 —
> and it matters more now than it did, because legacy and recovered are the two arms of the CI A/B
> and are downloaded side by side from one run.)

### Karma is the DEFAULT on every 32-bit target (2026-08-27)

`USE_KARMA_DECOMP` is **ON by default**. `native`, `windows`, `windows64`, `wasm`, `wasm-debug` and
`wasm-perf` all link the recovered Karma; the `*-nokarma` presets ask for none. There is no longer a
third state — see the note under "Karma" below.

**Android is the exception and it is deliberate.** `Android/app/build.gradle` passes
`-DUSE_KARMA_DECOMP=OFF` explicitly, because two of its three ABIs (`arm64-v8a`, `x86_64`) are LP64
and the recovered sources encode i386 struct LAYOUTS — 128 of 151 structs change size at 64-bit,
silently. The owner's call was to fix LP64 first rather than ship an APK whose physics depends on
which phone you have. Deleting that one line is the whole change once `tools/layout_check.py` is
clean.

CMakeLists enforces this rather than trusting it: `UT_PTR_BYTES` is computed once and a
`USE_KARMA_DECOMP` build on a 64-bit-pointer target is a **hard error**, not a warning.

> ### ✅ RE-VALIDATED IN ufront 2.55 — AND IT FOUND TWO REAL REGRESSIONS
>
> ### ufront 2.58 (2026-09-11) — both `-O2` items diagnosed; the pin stays by the owner's decision
>
> **1. The Karma `-O2` divergence is ONE FILE and ONE GCC PASS.** Complement bisect over all 146
> recovered sources (`UT_KARMA_O2_FILES`, below) names
> `MdtKea/keaCalcIworldandNonInertialForceandVhmf_vanilla.c` alone. Ghidra flattened its two 3x3
> matrix loops into `m1[0][iVar9 + k]` with `iVar9 = 0,3,6` on `MeReal m1[3][3]`, so iterations 2-3
> subscript past the inner array — undefined in C, exactly what the machine code does. GCC's
> value-range propagation (on at `-O2`, off at `-O1`/`-Og`) uses that to cut each loop to one
> iteration; rows 2-3 of the world inertia tensor are never written; the first constrained body solves
> wrong at frame 1. GCC even says so — `iteration 1 invokes undefined behavior
> [-Waggressive-loop-optimizations]` — but the decomp builds `-w`. Measured, all on the reference
> scenario: every other probe (`-fwrapv`, `-ftrivial-auto-var-init=zero`, `-fno-inline`,
> `-fno-tree-dse`, `-ffp-contract=off`, `-fno-ipa-*`) still diverges; `-O1`, `-Og` and
> `-O2 -fno-tree-vrp` match; the file's ten subscripts respelled `((MeReal *)m1)[k]` make **all 146
> files at `-O2` byte-identical to the reference**, and that respelling compiles to a byte-identical
> wasm object under the shipping `-O3 -g0` flags. The SDK's amd64 build of the function compares its
> counters to 3 twelve times. **The pin stays** — measured, `-O2` Karma buys nothing on ONS-Torlan
> with 6 bots (600 fixed frames: 3.6 s vs 3.7 s) — and the repair is recorded in the decomp repo's
> `proven.txt` (`O2-VRP-FLATTENED-INDEX`) for whenever it is worth landing as a post-pass.
>
> **2. The WEB was measured, not argued.** `packages/e2e/tools/ktrace-probe.cjs` (restored) drove
> the shipped `-O3` wasm through the same 40 frames with `-KTRACECONTACTS`: 600 body rows, K=1396,
> two runs identical, 13 of 15 bodies bit-identical to the reference, the two vehicles first
> differing at frame 13 by < 2e-6 (libm). Clang does not draw the inference GCC draws. The pin
> fixed the arm nobody ships, and the arm that ships was never broken.
>
> **3. `Source/PixoDrv` at `-O2` — ROOT-CAUSED AND FIXED (Linux).** `utils_surface.h` typed the
> blit/stretch pointer `__stdcall` unconditionally while `pixomatic_linux.a`'s entry points are
> cdecl, so every `psurf->scale_func(...)` in `scale_buffer()` left `esp` 40 bytes low. `-O0` hid it
> (`leave` restores `esp` from `ebp`); `-O2` omits the frame pointer, the epilogue restored
> `ebx/esi/edi/ebp` from the ARGUMENT area and `ret` popped a framebuffer pointer — a jump into the
> `READ_IMPLIES_EXEC` heap. Under gdb: pc in `[heap]`, `esp` inside `.data`, `ebx == 0x1400`, the
> 1280-pixel ARGB pitch passed as `dpitch`. Armed by `Zoom2X=True` (the ini default):
> `PIXO_BUF_LOCK_COMPOSITE` → `PixoSurface_BufferLock` → `scale_buffer`; with `Zoom2X=False` the same
> `-O2` binary ran clean. Fix: the typedef is `__stdcall` only under `_WIN32`. Verified at `-O2`,
> `Zoom2X=True`: 40 traced frames, 600/1396, a rendered 1280x720 frame grab, and MATCH 15/15 against
> the archived x87 trace. The configure WARNING is gone. `UT_PIXODRV_O0` (in `Source/PixoDrv/CMakeLists.txt`)
> is the owner's fallback pin, OFF.
>
> **4. Windows 32-bit with Pixomatic — the second GPU-free target, two defects fixed, one open.**
> The exe RUNS under wine: the 2.54 note said it did not, but that was the box's default `wine`
> (wow64-only, exit 53); the i386 loader `/usr/lib/i386-linux-gnu/wine/wine` with a `WINEARCH=win32`
> prefix runs it, and with `-NULLRENDERER` it traces 600/1396, MATCH 15/15 against the Linux x87
> build. With `-SOFTWARERENDERER` it died in `pixomatic.dll+0x1d639` with
> `EXCEPTION_INT_DIVIDE_BY_ZERO` from `CreatePixoBuffers` → `PixoBufferOpen`, because
> `USDL2Viewport::GetWindow()` returns the `SDL_Window*` and the DLL was handed it as an `HWND`;
> `SetRes` now resolves the real HWND through `SDL_GetWindowWMInfo`. Past that it died three frames in
> with a NATIVE segfault — which was the harness, not the game: `ktrace_run.sh` ran Xvfb at 640x480
> under a 1280x720 window, and wine's GDI cannot blit a window larger than the desktop (SDL2/X11 on
> Linux does not care). On a 1280x720 screen the win32 Pixomatic exe traces **40/40 frames,
> 600/1396, twice, byte-identical to its own `-NULLRENDERER` run**, and renders (frame grab). NX
> was not involved — an exe without `--nxcompat` crashed the same way on the small screen and the
> shipped exe keeps `NX_COMPAT`. Both harnesses run a 1280x720 screen now. Under real Windows it is
> still unmeasured; under wine it is the second GPU-free target.
>
> **6. The software renderer's mid-match crash — DIAGNOSED AND FIXED, and it WAS an `-O2` question
> after all (engine side).** `-SOFTWARERENDERER` died ~1-3 minutes into ONS-Torlan with bots in 5 of
> 8 runs (96-192 s), surfacing in `USkeletalMeshInstance::UpdateAnimation`, in `McdModelGetGeometryType`
> under `KHandleCollisions`, and inside glibc's `realloc` — the shape of memory corruption — while
> `-NULLRENDERER` ran 300 s every time. The `Zoom2X` composite path was NOT it (one pass, then a crash at
> 136 s); the 32-entry stream-definition array handed to the JIT was NOT it (sized, `check()`ed, peak
> never above 32); glibc's malloc checker stayed silent (the corruption sits inside objects, not chunk
> headers); a hardware watchpoint on the class-default block never fired. What the instruments finally
> agreed on, all measured on ONS-Torlan, 300 s, bots, `-SOFTWARERENDERER`:
>
> ```
> engine -O2, PixoDrv -O2 or -O0 ............ CRASH 5 of 8   (so PixoDrv's own -O is irrelevant)
> engine -O0 (the Debug probe build) ........ PASS  4 of 4
> UnSkeletalMesh.cpp alone at -O1 ........... PASS  3 of 3
> UnSkeletalMesh.cpp alone -fsanitize=undefined  PASS 1 of 1 — and it named TWO sites, both
>     "reference binding to null pointer": ComputeSkinVerts:3486 `&Duplicates(0)` and
>     GetFrame:4201 `&TrafoHeap(0)`, each on an EMPTY TArray (Data == NULL)
> those two sites guarded, everything -O2 .... PASS  4 of 4, growth canaries silent
> UnSkeletalMesh.cpp -O2 -fno-delete-null-pointer-checks / -fno-tree-vrp .. PASS 3 of 3 each
> after the fix, every arm rebuilt: 64-bit trace BYTE-IDENTICAL to the SSE control, win64 gate PASS,
>     Linux 32 Pixomatic 300 s 2/2, win32 Pixomatic 300 s + trace == its null-renderer trace, and the
>     rebuilt wasm's browser trace BYTE-IDENTICAL to the shipped one (600/1396, ece9a79dee94)
> ```
>
> The mechanism, as far as it was measured: a trap in `FArray::Realloc` caught `USkeletalMeshInstance::Blends`
> growing ONE ELEMENT AT A TIME through the exact geometric capacity sequence (33, 78, 140, 225, 342,
> 503, 725, 1030 …) inside a single `ValidateAnimChannel(0)` call at map load (`PostNetBeginPlay` →
> script `GetBoneCoords` → `GetFrame`), on to ~159k entries — 17.8 MB per instance, then copied into
> every later instance, then walked by `UpdateAnimation` off the end. The compiled loop is correct
> and bounded by the channel argument; the caller (`GetFrame`, with the bounds-checking wrapper
> inlined) is where `&TrafoHeap(0)` tells GCC a pointer it knows is NULL was dereferenced, which makes
> that path unreachable in the optimiser's model and licenses it to merge control flow — consistent
> with the clone receiving a foreign local as its bound, and with every mitigation above. Why only
> the software renderer reaches the empty-`TrafoHeap` state at that call is NOT explained; it is
> recorded as data-dependence, measured, not understood. The fix is the two guards (`Num() ? &X(0) : NULL`),
> which are also simply correct C++. `UT_ENGINE_TU_FILES`/`UT_ENGINE_TU_OPTIONS` in
> `Source/Engine/CMakeLists.txt` is the per-source knob that found it, kept.
>
> **5. The control arm is a preset now** — `native-sse` (`UT_FPMATH_SSE=ON`). Before 2.58 the
> `-msse2 -mfpmath=sse` flags were passed by hand and never reached the cache: `build-b32-sse` was an
> SSE binary (DWARF producer, 605 SSE / 1 x87 in `MdtBclAddAngular3`) in an x87 build tree, and one
> rebuild would have silently regressed it. The `windows64` preset also could not configure at all
> (it inherited `USE_PIXOMATIC=ON` into the 64-bit guard); it states `OFF` now.
>
> **The knobs** (top-level `CMakeLists.txt`, Karma pin block): `UT_KARMA_O2_FILES` — a regex over
> decomp source basenames compiled with `UT_KARMA_O2_OPTIONS` (default `-O2`) despite the pin; `.*`
> is the everything-`-O2` arm, a narrower regex one bisect round, `-O2;-fno-tree-vrp` or
> `-O2;-fsanitize=undefined` a probe. `UT_KARMA_PIN_WEB` lifts the `NOT EMSCRIPTEN` guard for a
> measurement build (OFF; the shipped wasm stays `-O3`).
>
> 2.54 restored optimisation to the native builds and said "re-validate rather than assume". The
> re-validation was done, and assuming would have been wrong twice.
>
> **1. The recovered Karma C cannot be optimised — the engine around it can.** Measured on
> `test-karma-1`, 40 frames, Onslaught, `-KTRACECONTACTS`, x86-64:
>
> | build | body rows | K | md5 |
> |---|---|---|---|
> | everything `-O0` | 600 | 1396 | `c31ed77b7323` ← the reference |
> | **Karma C `-O0` + engine C++ `-O2`** | **600** | **1396** | **`c31ed77b7323`** |
> | everything `-O2` | 543 | 1074 | `179ace20fb15` |
>
> Optimising the ENGINE changes nothing. Optimising the RECOVERED code changes the simulation, and
> not by a rounding digit — the traced body-row count itself drops. `CMakeLists.txt` now pins the
> `metoolkit_decomp` targets to `-O0` on native/Windows only, and the gate passes at the reference
> value with the engine still at `-O2`. ⚠ Not strict aliasing: the decomp already builds with
> `-fno-strict-aliasing`, so that was never a candidate; the specific transformation is unidentified.
> ★ It probably costs little: this source was **decompiled from an already-optimised build**, so the
> optimiser's work is baked into its shape. That is a prediction — measure before trusting it.
>
> **2. `Source/PixoDrv` SIGSEGVs at `-O2` (was OPEN — FIXED in 2.58, see above: a stdcall/cdecl mismatch in `utils_surface.h`).** The 32-bit software renderer crashes
> during renderer init; `-O0` is clean, `-O2` is a deterministic fault, and `-O2` everywhere *except*
> that one directory is clean. The fault is a jump into heap data executing garbage — a corrupted
> code pointer — and it is not the JIT allocator, which is never reached. Pixomatic itself is a
> prebuilt archive `-O2` cannot touch, so the defect is ours. Configure now warns. Use
> `-NULLRENDERER` (physics traces identically) until it is fixed.
>
> **The gate, after the fix** — both arms, exactly reproducing the 2026-09-02 result:
>
> ```
> 64-bit vs 32-bit SSE control : self-consistent, BYTE-IDENTICAL (c31ed77b7323, K=1396)
> windows64 under wine         : self-consistent, differs ONLY at the 1e-15..1e-19 libm floor
> ```
>
> ⚠ **The control must be a 32-bit `-msse2 -mfpmath=sse` build**, as `ktrace_gate.sh`'s own docstring
> says — *not* the `native` preset. `native` is `-march=pentium -msse` with no `-mfpmath=sse`, so its
> doubles run on x87 with 80-bit intermediates and it produces `81b06b0cf715`, never the reference.
> `decomp/test/README.md` pointing at `build-native` is wrong for this purpose.
>
> ⚠ **The WEB IS NOT AFFECTED and was never part of this.** `wasm-perf` builds Karma at `-O3` and the
> dev/`-debug` wasm at `-O2`, and always did — 2.54's `else()` only ever caught native. Verified after
> this change: the web configure applies no pin, its Karma C is still `-O3`, and the built
> `SDLLaunch.wasm` is byte-identical to the shipped one.
>
> ### The 2.54 note this replaces — NATIVE BUILDS WERE SILENTLY -O0

>
> **This is kept because the consequence below is still live.** The optimisation block used to end in
> an `else()` marked "Match Phase 1's -O0 debug-oriented build for now", and every native configuration
> landed there — RelWithDebInfo included, because the only test was `CMAKE_BUILD_TYPE STREQUAL
> "Debug"`. The flag line read `… -O2 -g -DNDEBUG … -m32 -O0 -g …` and the last `-O` won, so the engine
> AND `KarmaDecomp` compiled unoptimised on Linux and Windows.
>
> `CMakeLists.txt` now states one `-O` per configuration and blanks the per-config `CMAKE_<LANG>_FLAGS_<CONFIG>`
> variables that were injecting the competing one, so RelWithDebInfo really is `-O2 -g` and Release
> really is `-O3 -g0`. Verified with `compile_commands.json`: the native line went from
> `['-O2','-O0']` to `['-O2']`, with `-DNDEBUG`/`-D_NDEBUG=1` unchanged — the optimisation level is the
> only thing that moved.
>
> ⚠⚠ **SO THE WIN32-PORT-3 HAZARD BELOW IS NOW REACHABLE BY DEFAULT.** Native and Windows builds were
> getting -O0 by accident; they now get what their preset asks for. Re-validate rather than assume.
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
32-bit x86 archives, so linking THOSE can never work off x86 and the web build had always been
`NO_KARMA` — no vehicles, no ragdolls. `metoolkit-karma-decomp` recovered those archives as ordinary
portable C, which compiles for wasm32 like anything else in the tree.

The presets differ from plain `wasm-debug` / `wasm-perf` by **one cache variable**,
`USE_KARMA_DECOMP=ON`, which does two things:

1. **Compile side.** `WITH_KARMA=1` globally (so `NO_KARMA` is
   *not* defined and the eleven `Source/Engine/Src/K*.cpp` bridge files are compiled), plus
   metoolkit's public headers on the include path. Those headers are portable C and were never the
   problem.
2. **Link side — a CMake target instead of a glob of archives.** `Source/Engine/CMakeLists.txt`
   builds a `KarmaDecomp` static library from ``metoolkit_decomp/src/<Lib>/*.c`` (145 recovered
   objects, checked in — see `metoolkit-karma-decomp `metoolkit_decomp/README.md``) plus the hand-written convex-hull
   replacement ``decomp/src/McdConvexCreateHull/kd_convexhull.c``.

### ⚠ The legacy Karma is LINUX-ONLY, and Windows was tried (2026-09-12)

A `windows-legacykarma` arm existed for a few hours on 2026-09-12 and was removed the same day.
Recording it so nobody re-derives it:

* **The long-standing reason was wrong.** The note beside `ME_FORCE_CDECL` said MathEngine's Windows
  archives were "32-bit Linux ELF" and therefore unlinkable. They are not:
  `metoolkit/lib.rel/win32_single_msvcrt/*.lib` are real MS COFF archives whose Karma entry points
  are plain `__stdcall`-decorated C (`_McdNullCreate@4`).
* **It links, and it boots.** It needed six MSVC compiler-support routines MinGW does not provide
  (`_chkstk`/`_alloca_probe`, `_alldiv`/`_aulldiv`/`_allmul`, `_except_list`, plus `??3@YAXPAX@Z`),
  and the vendor archives kept out of `--export-all-symbols`. The resulting exe loads a map, starts
  a match and simulates Karma.
* **★ And then GNU ld's COMDAT handling kills it.** At `McdSphylSphereIntersect+0x9d` the object
  carries `DISP32 ?MeVector3Dot@@YGMQBM0@Z`; the linked image carries `e8 00 00 00 00` —
  displacement **zero**, so the `call` targets the *next instruction*. It pushes a return address
  nothing pops, ESP stays 4 bytes low for the rest of the function, and every `N(%esp)` after it
  reads the wrong slot. The observed crash is a geometry pointer read out of a slot holding a
  float, exactly as that predicts, and the bogus return address `0x00a84662` is sitting in the
  crash dump's stack scan.
* **⚠ It is not a linker-flag problem, and that was checked rather than assumed.** There are **42**
  such sites — `McdBoxBoxIntersect`, `McdGjkTest`, the triangle-list intersectors, i.e. the core of
  collision detection — and the count and the bytes are **identical** with `--exclude-libs` and
  `--export-all-symbols` both removed. ld binds a COMDAT kept from the *same* object and fails
  across objects. Each site is a silent stack corruption, so "it mostly works" is the dangerous
  reading, not a mitigation.
* **What was checked and cleared first**, so the search is not repeated: the hand-written `_chkstk`
  (unit-tested across page boundaries, ECX preserved — correct); `sizeof(McdContact)` (40, and
  MathEngine's own code uses `add $0x28` — identical); `McdModelPair` / `McdIntersectResult`
  layouts; and the engine's own out-of-range geometry-type diagnostic, which never fired.
* **Baseline discipline saved a wrong conclusion.** The exe has 4804 `U` symbols and 544
  "call-to-next-instruction" sites — and the *working* recovered Windows exe has **more** of both
  (4661 / 585). Those aggregates are artifacts of `--export-all-symbols` and objdump desyncing on
  data in `.text`. Only the symbol-anchored single site is evidence.

The engine now **refuses** `BUILD_KARMA_REF` on Windows with a `FATAL_ERROR`, because the
configuration compiles, links and boots — without the guard the next person gets a plausible binary
that corrupts its own stack. Git history has the preset and the shim if binutils ever fixes this.

⚠ **`BUILD_KARMA_REF` was removed on 2026-09-02 and RESTORED on 2026-09-12.** It links MathEngine's
prebuilt archives as a second implementation of this same API. The removal's reasoning — that a
proven recovery needs no second implementation in the engine, and that `metoolkit-karma-decomp`'s
`difftest_pair.sh` / `substitute_test.sh` compare per object without one — still holds for the
*decompilation*; what it missed is the *shipped build*, which is what the owner asked to keep
proving per push. It is now the CI A/B arm (`legacy-karma`, Linux only — see above) and it replaced
`native-nokarma` there. **32-bit x86 only**: the archives are binary-only i386, and the configure
step fails by name rather than letting the link fail twenty minutes in.

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
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
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
WINEPREFIX=<prefix> wine64 ./ut2004.exe \
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
| `-OPENGLRENDERER`  | `OpenGLDrv.OpenGLRenderDevice` | `SDL2Drv.SDL2Client` | `native*`, `physics`, `windows*` |
| `-SOFTWARERENDERER`| `PixoDrv.PixoRenderDevice`     | (keep current)       | `native*`, `physics` (`USE_PIXOMATIC=ON`, never on web or Windows) |
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

## Shared with the other engines of the frontend (2026-10-11, from engine-uc2)

The same commands, settings and log lines in every engine (ufront's UC2 merge; engine-uc2 is the same engine
family, and its `docs/CHEAT-COMMANDS.md` is the reference for the cheats):

- **`WHEREAMI`** prints `<x> <y> <z> <yaw> <pitch>` (the pawn's location, one decimal; the controller's view yaw
  0..65535 and pitch -32768..32767) on the console, on screen and in the log; **`TELEPORT <x> <y> <z> [<yaw>
  [<pitch>]]`** puts the local player there (WHEREAMI's output pasted back gives the same place and view). `TELEPORT`
  with no arguments is still Epic's `CheatManager.Teleport` (to the surface looked at). Cheats: allowed in a
  standalone game and for a listen server's host, refused (logged) on a network client; `[Engine.GameEngine]
  AllowNetCheats=True` opens them for test harnesses only (`-ini:`), never in a shipped ini. Every use logs a `Cheat: `
  line. `Source/Engine/Src/UnCheats.cpp`, first in `UPlayer::Exec` (the console runs the scripts' exec functions
  before the engine's commands) and in `UGameEngine::Exec` (the web's `ue_exec_command` reaches the engine only).
- **`[Engine.GameEngine] FpsLogSeconds=<s>`** (0 = off) / console **`FPSLOG <s>`**: an `FPS:` log line every `<s>`
  seconds (frames, fps, frame ms avg/min/max/p99, the 1% low, the viewport size). `Source/Engine/Src/UnFpsLog.cpp`.

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
(CMake), which only the NDK toolchain sets — `emcmake` and the native presets never do.

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
JAVA_HOME=<android-studio>/jbr ./gradlew :app:assembleDebug   # -> app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.ut2004.android/.GameActivity                # SDL_main runs the engine; logcat tag UT2004
```

- **Status (2.10a — build standup):** builds + links + installs + launches; `SDL_main` runs the
  engine into `appInit`, which stops at `MissingIni` (no game data yet — external-storage access is
  **2.10b**). arm64-v8a, file management, gamepad, touch, net, and lifecycle follow — see the
  monorepo `docs/migration/steps/2.10-android.md`.

## Assets (`UT_ASSETS_DIR`)

The WASM link **preloads 4 INIs** into MEMFS (needed during `appInit`): `System/{Default.ini,DefUser.ini}`
from the tracked `Engine/Config/web`, and `System/{Build.ini,Packages.md5}` from `$UT_ASSETS_DIR`:

```bash
cmake --preset wasm -DUT_ASSETS_DIR=/abs/path/to/ut2004-assets
```

The native presets do **not** read assets at build time (the
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

Karma has **three** states again since 2026-09-12: **`USE_KARMA_DECOMP=ON`** (the recovered sources
— the default everywhere the pointer width allows), **`BUILD_KARMA_REF=ON`** (MathEngine's prebuilt
32-bit x86 archives — the `legacy-karma` A/B arm, **Linux only**; removed 2026-09-02, restored
2026-09-12), and **both OFF** (`NO_KARMA=1`, the `*-nokarma` controls). Pixomatic is
`USE_PIXOMATIC` — on for every native preset and for 32-bit Windows since 2.54.

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
