# engine-ut2004 docs

Engine-repo documentation. Build/toolchain details are in [`../BUILD.md`](../BUILD.md); agent
conventions in [`../CLAUDE.md`](../CLAUDE.md).

## Contents
- [`../BUILD.md`](../BUILD.md) — repository layout, build matrix, presets, `UT_ASSETS_DIR`, GL4ES fork, toolchain.

## Repository layout (2026 reorg)
The tree was reorganized to mirror the sibling engine repos: `Source/` (engine C++), `Thirdparty/`
(vendored deps: zlib, Vorbis, Speex, OpenAL, SDL12, metoolkit, Pixomatic), `Engine/Config/` (reference
boot inis), `Android/` (NDK project), `vendor/glue/` (monorepo ABI shim), plus `cmake/` + `docs/`. The
retail `System/` runtime dir and `Content/` game-content archive were removed (redundant with the
out-of-tree asset tree at `$UT_ASSETS_DIR`); the engine builds from `Source/` + `Thirdparty/` only.
Full table in `../BUILD.md`.

## Done
- **Bloat removal** — the bundled `DirectX8/9/9_AMD64` SDKs, the `Microsoft Platform SDK for Windows
  XP SP2`, retail installers/tools (`Help/`, `Manual/`, `Tools/`, `RemoteDebugging/`, `unxutils/`),
  and the dead pre-CMake MSVC/makefile build have been removed. The kept `WinDrv`/`D3DDrv` Windows
  code now uses system SDK includes. See git history on the reorg branch.

## Deferred (placeholders — to be written when the work happens)
- **Porting story** — the high-level changes it took to run UE2.5/UT2004 on the web (main-loop
  inversion via Asyncify, FetchFS-backed file layer, WebRTC net driver, GL4ES→WebGL). Today this
  history lives in the old monolith's notes; it will be rewritten fresh here, not copied verbatim.
- **Physics/renderer blobs** — the closed prebuilt libs now live under `Thirdparty/{metoolkit,Pixomatic}/`;
  build with them via `-DBUILD_KARMA_REF=ON` (Karma) / `-DUSE_PIXOMATIC=ON` (Pixomatic software renderer).
  The default `gl4es`/`wasm` builds use neither.

## Where the migration plan lives
This repo was created by **step 0.1** of the ufront re-architecture. The phased plan and durable
cross-context state are in the **monorepo**, not here:
`/home/ion/ufrontend/docs/migration/{STATE.md,ROADMAP.md,steps/}`.
