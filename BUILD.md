# Building engine-ut2004 (native + WebAssembly)

This is the **private engine repo** for the UT2004 (UE2.5) web port — engine source only. Game
assets (~5.8 GB: maps, textures, meshes, sounds, animations, compiled `.u` packages) live **out of
tree** at `$UT_ASSETS_DIR` (default `/home/ion/ut2004-assets`) and are never tracked here.

> Status: the heavy browser glue has been **extracted** to the public monorepo (`@ufront/glue`,
> Phase 1 steps 1.3/1.4) and is injected at runtime — the WASM build bakes only the thin `webrtc_*`
> shim (`vendor/glue/webrtc-shim.js`). As of **step 1.6a** the host page is **un-baked** too: the
> link drops `--shell-file` and uses `SUFFIX .js`, so the build emits a **bare loader**
> (`SDLLaunch.{js,wasm,data}`, `loaderStyle=global-module`, **no `SDLLaunch.html`**); the host page
> lives in the public monorepo (`packages/frontend/index.html`). The now-dead `web/shell.html` (and
> the dead `web/{fetchfs,webrtc-net,idb-*}.js` + their tests) are slated for the ufront **1.9**
> cleanup gate.

## Repository layout

| Dir | What |
|-----|------|
| `Source/` | all engine C++ — Core, Engine, Fire, IpDrv, XGame, XInterface, UWeb, Onslaught, ALAudio, the render/viewport drivers, the `SDLLaunch`/`UCC` launchers, plus the kept Windows/editor/tool code. The code the build compiles. |
| `Thirdparty/` | bundled deps: `zlib`, `Vorbis`, `Speex`, `OpenAL`, `SDL12` (headers + the legacy `libSDL-1.2.so.0`), `metoolkit` (Karma), `Pixomatic`. SDL2 + GL4ES are FetchContent'd, not vendored. |
| `Engine/Config/` | reference snapshot of the web boot inis (`Default.ini`, `DefUser.ini`); **not** build-consumed. |
| `Android/` | the NDK/Gradle project; AGP wraps the root `CMakeLists.txt`. |
| `vendor/glue/` | the monorepo ABI contract baked into the web build (`ue_abi.h`, `webrtc-shim.js`). |
| `cmake/`, `docs/` | CMake modules; this repo's docs. |

Build config is the root `CMakeLists.txt` + `CMakePresets.json`.

## Build matrix

The native engine builds in **two mutually-exclusive graphics backends**, each a **hard split**
(its own binary in its own locked build dir, one viewport driver + one render driver — never
shared), plus the Emscripten/WASM target.

| Preset | Viewport | Renderer | Build dir | Output |
|--------|----------|----------|-----------|--------|
| `legacy` | SDL 1.2 (`SDLDrv`) | desktop OpenGL (`OpenGLDrv`) | `build-legacy/` | `Source/SDLLaunch/ut2004-legacy.bin` |
| `gl4es` | SDL 2 (`SDL2Drv`) | GL4ES → GLES2 (`GL4ESDrv`) | `build-gl4es/` | `Source/SDLLaunch/ut2004-gl4es.bin` |
| `wasm` | SDL 2 (port) | GL4ES → WebGL | `build-wasm/` | `Source/SDLLaunch/SDLLaunch.{html,js,wasm,data}` |
| `wasm-perf` | SDL 2 (port) | GL4ES → WebGL | `build-wasm-perf/` | `Source/SDLLaunch/SDLLaunch.{html,js,wasm,data}` |

Extra presets: `physics` (gl4es + Jolt), `karma-ref` (gl4es + 32-bit Karma reference). The
**web-first** requirement for this repo is that **`gl4es` (native) and `wasm` both build green**;
`legacy` is best-effort reference.

### Why the hard split
- **SDL 1.2 and SDL 2 cannot coexist** in one binary (same symbol names, incompatible state).
- **`OpenGLDrv` and `GL4ESDrv` cannot coexist** in one translation unit (each pulls a different
  `GL/gl.h`). Separate binaries + separate build trees is the only reliable arrangement.

Each build dir is **locked** to its backend on first configure (`UT_GFX_BACKEND_LOCKED`);
reconfiguring a folder for the other backend is a hard CMake error — use a separate folder.

## Render-device selection (`-*RENDERER` switches)

`SDLLaunch.cpp`'s `SelectRenderDevice()` (ufront 2.9) picks the render device from a single command-line
switch, native **and** web. Each switch names a render device and (for the two GL drivers, coupled 1:1 to
an SDL viewport manager) its matching `ViewportManager`:

| Switch | Render device | Viewport | Available in |
|--------|---------------|----------|--------------|
| `-GL4ESRENDERER`   | `GL4ESDrv.GL4ESRenderDevice`   | `SDL2Drv.SDL2Client` | `gl4es`, `wasm`/`wasm-perf` (default) |
| `-OPENGLRENDERER`  | `OpenGLDrv.OpenGLRenderDevice` | `SDLDrv.SDLClient`   | `legacy` |
| `-SOFTWARERENDERER`| `PixoDrv.PixoRenderDevice`     | (keep current)       | none (Pixomatic never built) |
| `-NULLRENDERER`    | `Engine.NullRenderDevice`      | (keep current)       | all presets |

- Availability is a `StaticFindObject` on the class' short name — a device statically linked into this
  preset registered its `UClass` at startup. A switch for a driver **not** compiled into the current
  preset (the legacy↔gl4es split is build-time exclusive; Pixomatic is never built) logs
  `render device X not available` and **no-ops** — the game still boots on the default (the canary). This
  also fixes the old latent `-SOFTWARE`→`PixoDrv` fatal (it set an uninstantiable device, then aborted).
- With **no** switch, the render device is left to the shipped ini + the existing `InitEngine()`
  auto-switch guards (`WinDrv`→SDL, `D3D`→OpenGL, `OpenGL`/`GLES3`→`GL4ES` on SDL2) — those guards already
  normalize the persisted/default choice to the built stack, so `SelectRenderDevice()` doesn't duplicate
  or fight them (the no-switch boot is unchanged).
- A switch that survives `Engine->Init()` is persisted to the ini (`PersistRenderChoiceIfSwitched()` →
  `GConfig->Flush`) so the pick sticks next boot (on the web, via the user-data IDB layer). The frontend's
  per-target `videoDrivers` DATA maps a friendly label → the switch.

## Toolchain
- **gcc-13 / g++-13** (pinned by the presets). Newer GCC is unvalidated against this pre-C++11 tree.
- **32-bit (`-m32`)** throughout — needs `gcc-multilib` + i386 X11/GL dev libs for native.
- **CMake ≥ 3.21** (tested on 4.2). GL4ES declares an ancient `cmake_minimum_required`; the build
  forces a modern policy floor so it configures on CMake 4.x.
- **Emscripten 5.0.x** for the WASM presets (validated on emsdk 5.0.7). `source ~/emsdk/emsdk_env.sh`
  first — the `wasm` presets read `$EMSDK` for the toolchain file.
- **Android NDK r30** (x86_64 first) drives the separate `Android/` NDK build via Gradle (SDK clang +
  cmake) — it is **not** a CMake preset; see the **Android** section below.

## Quick start

```bash
# --- Native: GL4ES (the WASM stack's native twin) ---
cmake --preset gl4es
cmake --build --preset gl4es -j"$(nproc)"        # -> build-gl4es/Source/SDLLaunch/ut2004-gl4es.bin

# --- Native: legacy reference (best-effort) ---
cmake --preset legacy
cmake --build --preset legacy -j"$(nproc)"        # -> build-legacy/Source/SDLLaunch/ut2004-legacy.bin

# --- WebAssembly (dev/debug: -O0 + assertions + DWARF) ---
source ~/emsdk/emsdk_env.sh
cmake --preset wasm
cmake --build --preset wasm -j"$(nproc)"          # -> build-wasm/Source/SDLLaunch/SDLLaunch.{html,js,wasm,data}

# --- WebAssembly (max perf: -O3, no debug info, smaller/faster) ---
source ~/emsdk/emsdk_env.sh
cmake --preset wasm-perf
cmake --build --preset wasm-perf -j"$(nproc)"     # -> build-wasm-perf/...
```

## Android (native NDK build — x86_64 first; ufront 2.10a)

A separate **native NDK/Gradle** build wrapping the engine as `libUT2004.so`. It reuses the `gl4es`
stack (SDL2 + `GL4ESDrv` → native EGL GLES2) — **not** wasm. **The web build is priority #1 and stays
byte-identical:** every Android delta is behind `#if defined(__ANDROID__)` (C++) / `if(ANDROID)`
(CMake), which only the NDK toolchain sets — `emcmake` and the gcc-13 presets never do.

- **Where:** `Android/` (Gradle 8.7 / AGP 8.5.2; `GameActivity extends SDLActivity`; vendored SDL
  **2.32.11** Java layer, version-matched to the engine's `release-2.32.x` SDL FetchContent).
- **Integration (Option B):** AGP's `externalNativeBuild.cmake.path` points at THIS repo's root
  `CMakeLists.txt` (`targets SDLLaunch`); the `if(ANDROID)` branches build `SDLLaunch` as a SHARED
  `libUT2004.so` (whole-archive, same package set as `gl4es`). No wrapper CMake — keeps
  `CMAKE_SOURCE_DIR` = repo root, so the existing `${CMAKE_SOURCE_DIR}` refs + the single FetchContent
  hold.
- **Android divergences from the native `gl4es` preset:** 64-bit (drop `-m32` / `-DPLATFORM_32BIT` /
  x86 `-march`); clang leniency instead of gcc `-fpermissive`; the **ANSI `NO_UNICODE_OS_SUPPORT`
  path (like wasm)**, NOT `-fshort-wchar`/UNICODE — bionic's 32-bit `wchar_t` + non-glibc `<wchar.h>`
  guard clash with the engine's 16-bit `wcs*` redeclarations; SDL2 built SHARED (`libSDL2.so`, aliased
  to `SDL2::SDL2-static`); stdout/stderr → logcat (`UT2004` tag; SELinux blocks
  `setprop log.redirect-stdio`). There is **no** `-fpack-struct=4` here (that was the UE1/UTA build) —
  packing stays `#pragma pack` under `-DSUPPORTS_PRAGMA_PACK=1`.

```bash
# NDK r30 + SDK cmake + Android Studio JBR 21. Emulator: x86_64 / API-34 (arm64 runs via translation).
cd Android
JAVA_HOME=/home/ion/android-studio/jbr ./gradlew :app:assembleDebug   # -> app/build/outputs/apk/debug/app-debug.apk (libUT2004.so + libSDL2.so)
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.ut2004.android/.GameActivity                # SDL_main runs the engine; logcat tag UT2004
```

- **Status (2.10a — build standup):** builds + links + installs + launches; `SDL_main` runs the engine
  into `appInit`, which stops at `MissingIni` (no game data yet — external-storage access is **2.10b**).
  arm64-v8a, file management, gamepad, touch, net, and lifecycle follow — see the monorepo
  `docs/migration/steps/2.10-android.md`.

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

The `gl4es` and `wasm` builds fetch GL4ES (via CMake FetchContent) from our **patched fork by
default**, because stock upstream `v1.1.6` renders environment-mapped (cubemap) surfaces wrong. The
fork fixes the active-unit cube bind and moves reflection-map texgen to the fragment shader. The
companion driver-side fixes live in `GL4ESDrv/`.

```bash
# Default — fork (nothing to pass):
cmake --preset gl4es
# Stock upstream (broken cubemaps; A/B only):
cmake --preset gl4es -DGL4ES_GIT_REPOSITORY=https://github.com/ptitSeb/gl4es.git -DGL4ES_GIT_TAG=v1.1.6
# Local checkout (GL4ES dev):
cmake --preset gl4es -DFETCHCONTENT_SOURCE_DIR_GL4ES=/abs/path/to/gl4es
```

- Default source: `https://github.com/10-p/gl4es`, branch `fix/cubemap-bind-active-unit`.
- The fork choice is interface-neutral — upstreaming later is just a default flip. Called out
  explicitly so it is never silently lost.

> The WASM build also FetchContent-clones GL4ES (and builds SDL2/OpenAL ports), so the **first**
> configure of a fresh build dir needs network access. Subsequent builds reuse `build-*/_deps/`.

## WASM dev vs perf

The only knob is `UT_WEB_PERF` (set by the preset). It gates compile flags (`-O0 -g` ↔ `-O3 -g0`)
and Emscripten link flags (`-g -gsource-map -sASSERTIONS=2 -sSTACK_OVERFLOW_CHECK=2` ↔
`-O3 -sASSERTIONS=0`). Everything else is shared: Asyncify, SDL2/OpenAL ports, `FULL_ES2`, memory
growth, the baked FetchFS/IDB/WebRTC JS libraries, the 16 MB temp-vertex-buffer-cap post-link fix.

Deliberately **not** used (to keep the perf build safe): **Closure** (would mangle the hand-written
JS glue baked via `--js-library`) and **LTO** (risks eliminating the engine's
`AUTO_INITIALIZE_REGISTRANTS_*` global constructors that `--whole-archive` preserves). `-g0` in the
perf path is load-bearing — *any* DWARF forces binaryen into limited-optimization mode and bloats
the wasm.

## Vendored third-party (`Thirdparty/`)

Bundled dependencies live under `Thirdparty/`: `zlib`, `Vorbis` (ogg+vorbis), `Speex`, `OpenAL`
(headers), `SDL12` (headers + the legacy `libSDL-1.2.so.0` runtime lib),
and the closed prebuilt libs `metoolkit/` (Karma physics) + `Pixomatic/` (software renderer). SDL2
and GL4ES are **not** vendored — they're fetched via CMake FetchContent. Karma is opt-in
(`-DBUILD_KARMA_REF=ON`; the `*-full`/`karma-ref` presets), Pixomatic is opt-in (`-DUSE_PIXOMATIC=ON`;
the `*-full` presets); the default `gl4es`/`wasm` builds use neither.

The former non-game bloat has been **removed** (see git history): the bundled `DirectX8/9/9_AMD64`
SDKs and the `Microsoft Platform SDK for Windows XP SP2` (the kept `WinDrv`/`D3DDrv` code now uses
system `<netfw.h>`/`<d3d8.h>` includes for a future Windows port), retail installers/tools (`Help/`,
`Manual/`, `Tools/`, `RemoteDebugging/`, `unxutils/`), the dead pre-CMake MSVC/makefile build, and
the retail `System/` runtime dir + `Content/` game-content archive (both redundant with the asset
tree). Nothing here is a build input beyond `Source/` + `Thirdparty/`; the engine builds/runs from
prebuilt `.u` + assets in `$UT_ASSETS_DIR`. (The one file the build needed from `System/`, the legacy
`libSDL-1.2.so.0`, now lives in `Thirdparty/SDL12/`.)

## Verifying the backend split (containment proof)

```bash
nm -C build-gl4es/Source/SDLLaunch/ut2004-gl4es.bin | grep -c UOpenGLRenderDevice   # 0
nm -C build-gl4es/Source/SDLLaunch/ut2004-gl4es.bin | grep -c UGL4ESRenderDevice    # > 0
```

## Unit tests (browser glue)

The browser glue and its Node test suite now live in the public monorepo (`@ufront/glue`); the
former in-repo `web/test/` suite and its `node_modules/` have been removed. Only the thin
`vendor/glue/webrtc-shim.js` shim is baked into the WASM build here.
