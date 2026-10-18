# engine-ut2004 — agent guide

The engine repository of the Unreal Tournament 2004 (Unreal Engine 2.5) port: engine source only. It builds the
native binaries (Linux, Windows, Android) and the WebAssembly artifact that the frontend repository
(`10-p/unreal-frontend-arch-private`) serves. Game assets are never in git.

- The baseline is the Epic drop in `~/WorkUnreal/epic-sources/` (`ut2004-v3369`; Pixomatic's driver from
  `ue2-pixomatic`), see its README.
- Features are listed in the frontend's docs/14-change-inventory.md; a new feature adds its line there.
- Building, presets, options and the layout: `BUILD.md`. What changed from the original source: `README.md`.

## Build

```bash
source /path/to/emsdk/emsdk_env.sh                           # the web presets read $EMSDK
cmake --preset native && cmake --build --preset native -j"$(nproc)"
cmake --preset wasm-perf -DUE_ASSETS_DIR=<UT2004 folder> && cmake --build --preset wasm-perf -j"$(nproc)"
```

- `cmake --list-presets` is the authority; `BUILD.md` describes each one.
- `UE_ASSETS_DIR` is required by the web presets: the link embeds `System/Build.ini` and `System/Packages.md5` from
  it. If it is a mount, check `ls $UE_ASSETS_DIR` before suspecting the engine.
- The first configure of a build directory fetches GL4ES (`10-p/gl4es`), SDL2, OpenAL Soft (`10-p/openal-soft-web`)
  and the recovered Karma (`10-p/metoolkit-karma-decomp`). `FETCHCONTENT_SOURCE_DIR_<NAME>` points one at a local
  checkout (offline builds, unpushed changes).

## Facts that constrain changes

- **One native stack.** SDL2Drv is the only viewport; OpenGLDrv, GL4ESDrv and Pixomatic (32-bit x86) link into one
  binary and `-OPENGLRENDERER` / `-GL4ESRENDERER` / `-SOFTWARERENDERER` / `-NULLRENDERER` picks one at startup.
  Drivers are chosen by the ini (`ViewportManager`, `RenderDevice`, `AudioDevice`, `NetworkDevice`).
- **Karma has three arms.** `USE_KARMA_DECOMP=ON` (the default, every target) links the recovered sources;
  `BUILD_KARMA_REF=ON` links MathEngine's prebuilt archives, 32-bit x86 Linux only (`legacy-karma`, the A/B
  reference in CI); `USE_KARMA_DECOMP=OFF` builds `NO_KARMA` (the `*-nokarma` presets). The Windows legacy-Karma arm
  is refused at configure: GNU ld drops the archives' cross-object COMDAT relocations and the collision code would
  run with a shifted stack. The recovered sources live in `10-p/metoolkit-karma-decomp`; a fix to them goes into
  that repository's generator, never into this one.
- **The web networking is two detached packages.** `WebRTCDrv` (peer to peer) and `ProxyNetDrv` (client-only,
  joins a native server through the WSS-to-UDP game proxy; `-DUE_WITH_PROXYNET=OFF` removes it) share no files.
  `vendor/glue/` is the engine-to-host ABI (`ue_abi.h`, version 2) and its shims, written by the frontend's
  `unreal-frontend-arch-private/packages/glue/bin/vendor-engine.mjs`: re-vendor, never edit by hand.
- **The frontend matches some log lines** (its E2E specs and `targets/ut2004/target.json`): the WebGL antialias
  line, `PendingLevel: server sends N file(s): ...`, the render-device and audio fallback lines among them. Grep
  `packages/e2e` and `targets/` there before changing a log line, and change both sides together.
- **A key-less install generates its own CD key** (`Engine/Src/UnCDKey.cpp`). The key alphabet and append string
  are proprietary: never copy them into the frontend (public), a log, a doc or a commit message.
- **Engine differences are data, not code**, on the frontend side: an engine never needs a frontend code branch.
- Build trees (`build-*/`) and game content stay out of git; the prebuilt blobs under `Thirdparty/` are tracked on
  purpose (no blanket `*.a`/`*.lib`/`*.dll` ignore).

## Rules for changes

- **Comments** say what the code does now, in one or two lines: no stage numbers or work ids, no story of a fix, no
  dates, no owner quotes, no measurements, no cross-references to other files' comments, no ★/⚠ or capitals for
  emphasis; a platform note sits inside its `#ifdef`. File headers say what the file is. Epic's and upstream
  comments stay as they are.
- **Docs** describe the current state in present tense, written from the code; every path, flag and preset they
  name exists. Records kept as history say so in their first line.
- **Names** follow behaviour; our CMake options take the `UE_` prefix. Ini sections and keys players save,
  `WebRTCDrv`/`UWebRTCNetDriver`, exported ABI symbols and frontend-matched log lines keep their names.
- **Commits** use Conventional Commit subjects naming the feature (`feat:`, `fix:`, `build:`, `refactor:`), never
  a stage number or a date.
