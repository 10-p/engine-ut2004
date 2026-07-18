# engine-ut2004 — agent guide

**What this is:** the **private** engine repo for the UT2004 (Unreal Engine 2.5) web port. It holds
engine source only and produces a WASM artifact (+ native binaries). It is **not** the product on
its own — the public monorepo `ufront` (`/home/ion/ufrontend`) consumes this engine's WASM artifact,
owns the host page / UI / build assembly, and (from Phase 1) owns the browser glue behind a thin
ABI shim.

## Map of the world
| Thing | Path | Role |
|-------|------|------|
| This engine repo (private) | `/home/ion/engines/engine-ut2004` | engine source → WASM/native artifacts |
| Public monorepo | `/home/ion/ufrontend` | consumes the artifact; host/UI/glue/build; **migration state lives here** |
| Game assets (never in git) | `/home/ion/ut2004-assets` | canonical ~5.8 GB; `$UT_ASSETS_DIR` |
| Old monolith (rollback only) | `/home/ion/ut2004-exps` | the pre-split repo; do not develop here |

> **Migration plan & durable state are in the monorepo**, not here:
> `/home/ion/ufrontend/docs/migration/STATE.md` (read it first if you are continuing the migration),
> `ROADMAP.md`, `steps/*.md`. This repo was created by **step 0.1**.

## Status (Phase 1, important)
The heavy browser glue is **extracted** to the public monorepo (`@ufront/glue`) and injected at runtime;
the WASM build bakes only the thin `webrtc_*` shim (`vendor/glue/webrtc-shim.js`) — steps 1.3/1.4. As of
**step 1.6a** the host page is **un-baked**: the link drops `--shell-file` and uses `SUFFIX .js`, so the
build emits a **bare loader** (`SDLLaunch.{js,wasm,data}`, `loaderStyle=global-module`, **no
`SDLLaunch.html`**); the host page lives in `ufront/packages/frontend/index.html`. The now-dead
`web/shell.html` + `web/{fetchfs,webrtc-net,idb-*}.js` (+ their tests) await the ufront **1.9** cleanup.

## Build (full details: `BUILD.md`)
```bash
cmake --preset gl4es && cmake --build --preset gl4es -j"$(nproc)"          # native twin
source ~/emsdk/emsdk_env.sh
cmake --preset wasm  && cmake --build --preset wasm  -j"$(nproc)"          # the web build
```
- **Green checkpoint for this repo:** both `gl4es` (native) and `wasm` build cleanly, producing
  `build-gl4es/SDLLaunch/ut2004-gl4es.bin` and `build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data,html}`.
- Assets path is `UT_ASSETS_DIR` (default `/home/ion/ut2004-assets`); only 4 INIs are preloaded.
- GL4ES is fetched from our **fork by default** (`10-p/gl4es@fix/cubemap-bind-active-unit`). First
  configure of a fresh build dir needs network (FetchContent clone of GL4ES + SDL2/OpenAL ports).
- Unit tests (glue, ride-along): `node --test web/test/*.test.js` → 168/168.

## Conventions / load-bearing facts
- **Hard backend split:** SDL1.2+OpenGLDrv (`legacy`) vs SDL2+GL4ESDrv (`gl4es`/wasm) never share a
  binary or build dir. Each build dir locks to its backend.
- **Drivers are selected by `.ini`, not compiled in** (`ViewportManager`/`GameRenderDevice`/
  `AudioDevice`/`NetworkDevice`). The web build's one engine-specific addition is `WebRTCDrv`.
- **Engine differences are DATA, not code** (the project-wide rule for the multi-engine future).

## Do NOT
- **Do NOT** recreate the in-tree content symlinks (`Maps`, `Textures`, `System/*.int`, … →
  `../ut2004-assets`). They were native-runtime conveniences in the old repo and are intentionally
  absent here; content is reached via `UT_ASSETS_DIR` (wasm) or a runtime dir (native).
- **Do NOT** commit build trees (`build-*/`) or game assets — both are gitignored.
- **Do NOT** add a blanket `*.a`/`*.lib`/`*.dll` ignore — vendored Karma/flex/DirectX blobs are
  intentionally tracked (see `.gitignore`).
- **Do NOT** make engine source changes for the relocation beyond the `UT_ASSETS_DIR`
  parameterization — that would be re-architecture, not Phase 0. Stop and re-scope.
