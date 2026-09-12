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
`SDLLaunch.html`**); the host page lives in `ufront/packages/frontend/index.html`. The `web/` directory
(`shell.html`, `fetchfs`/`webrtc-net`/`idb-*`, and their Node tests) is **gone** — the ufront 1.9 cleanup
has happened.

## Build (full details: `BUILD.md`)
```bash
cmake --preset native && cmake --build --preset native -j"$(nproc)"        # native twin
source ~/emsdk/emsdk_env.sh
cmake --preset wasm   && cmake --build --preset wasm   -j"$(nproc)"        # the web build
```
- **Green checkpoint for this repo:** both `native` and `wasm` build cleanly, producing
  `build-native/Source/SDLLaunch/ut2004-pixo.bin` and
  `build-wasm/Source/SDLLaunch/SDLLaunch.{js,wasm,data}`.
- **`cmake --list-presets` is the authority.** There is no `gl4es` preset and no `legacy` preset —
  both were retired with SDL 1.2 in ufront 2.34. The ten that exist are `native`, `native-karma`,
  `karma-ref`, `physics`, `wasm`, `wasm-debug`, `wasm-perf`, `windows`, and the two
  `wasm-karmadecomp-{debug,perf}`.
- **`wasm` is `-O2 -g`** (the dev default — named stacks and assertions at a playable speed);
  `wasm-debug` is the `-O0` last resort; `wasm-perf` is `-O3 -g0` shipping.
- Prerequisites for a from-scratch host (gcc-13, the i386 multiarch sysroot, emsdk 5.0.x) are in
  `BUILD.md` — a default install cannot build the 32-bit native presets.
- Assets path is `UT_ASSETS_DIR` (default `/home/ion/ut2004-assets`); only 4 INIs are preloaded.
  **They live on a mount that does not survive a reboot** — `ls /home/ion/ut2004-assets` before
  blaming the engine.
- GL4ES is fetched from our **fork by default** (`10-p/gl4es@fix/cubemap-bind-active-unit`). First
  configure of a fresh build dir needs network (FetchContent clone of GL4ES + SDL2/OpenAL ports).

## Karma — and it lives in ANOTHER repository now
Karma (MathEngine `metoolkit`) is UT2004's physics: Onslaught vehicles and ragdolls. It shipped as
**binary-only 32-bit x86 archives**, so the web and Android builds were always `NO_KARMA`.

**The SDK and its source-level recovery are
[`10-p/metoolkit-karma-decomp`](https://github.com/10-p/metoolkit-karma-decomp)** (ufront 2.44).
That repository holds the v3369 `metoolkit/` drop, the 145 recovered objects as
`metoolkit_decomp/`, the toolchain that produces them, and the gates that judge them. This
repository holds **only the engine-side bridge**: `Source/Engine/Src/K*.cpp`, `KarmaSupport.cpp`,
and the CMake that links it. Nothing of the recovery is vendored here —
the top-level `CMakeLists.txt` FetchContents it at `main`, unpinned.

## ✅ Karma is COMPLETE FOR UT2004 (2026-09-02)

Every code path this game exercises is recovered, repaired and measured, and every target it ships
on plays on the rebuilt physics: web (55/55), Linux 32 and 64, **Windows x64**, Android arm64 and
armv7. Decided by measuring which code the game ENTERS (660 functions, 111 of 147 objects) and
closing that set — the entered set has zero open defects.

⚠ **It is a UT2004 physics backend, not a validated general-purpose Karma SDK.** 36 recovered
objects the game never enters have never executed, `MdtLOD` carries two 64-bit defects that
`maxMatrixSize = 0x7ffffffc` makes unreachable here, and the convex hull is a replacement rather
than a recovery. Full list: that repository's `decomp/STATUS-EXEC.md`. Don't read those as "Karma is
unfinished", and don't read "Karma is done" as covering them.

**Before changing anything about the recovered physics, work in that repository, not this one**
— read its `CLAUDE.md`, then `decomp/docs/STATE.md` and `decomp/proven.txt` (the evidence ledger:
a non-comment line in it releases an object, and nothing is released without evidence).

**THREE states again as of 2026-09-12.** `BUILD_KARMA_REF` — MathEngine's prebuilt archives, linked
as an alternative implementation — was removed on 2026-09-02 and **restored** on 2026-09-12 at the
owner's request, because a shipped build should keep proving the recovery against the real thing on
every push. It brings back its mutual-exclusion `FATAL_ERROR`, the `-legacykarma` binary suffix and
`KarmaGlibcCompat.c`, and it is now the **A/B arm in CI**, where it replaced `native-nokarma`.

| | what links | presets |
|---|---|---|
| `USE_KARMA_DECOMP=ON` (**default**) | `metoolkit_decomp::all`, the recovered sources | every preset that can take it |
| `BUILD_KARMA_REF=ON` | MathEngine's shipped archives | `legacy-karma`, `windows-legacykarma` — **32-bit x86 only** |
| `USE_KARMA_DECOMP=OFF` | nothing; `NO_KARMA=1` | `native-nokarma`, `wasm-nokarma`, `windows-nokarma` |

★ **The legacy arm now builds on Windows too**, which the removal commit said was impossible ("the
Windows archives are 32-bit Linux ELF" — they are not; they are MS COFF with `__stdcall`-decorated
C entry points). It needs `Source/Engine/Src/KarmaMsvcCompat.c` for six MSVC compiler-support
routines MinGW does not provide, and the vendor archives kept out of `--export-all-symbols`.
⚠ **It BUILDS and BOOTS but does not survive a Karma-heavy scene** — access violation ~12 frames in,
inside MathEngine's own `McdSphylSphereIntersect`, undiagnosed as of 2026-09-12. The **Linux** legacy
arm is the one to cite: 40/40 frames, and it scores `MATCH` against the recovered arm.

It existed to be the A/B reference while the recovery was unproven, and the recovery is no longer
unproven: it is the physics on wasm (55/55), Linux x86-64, **Windows x64**, Android arm64 and
armv7, its 64-bit trace is byte-identical to the 32-bit control, and it plays a full i386 match
against a metoolkit with every shipped member deleted. ★ The recovered-vs-MathEngine comparison it
provided still exists and is **stronger** where it lives — `metoolkit-karma-decomp`'s
`decomp/test/standalone/difftest_pair.sh` and `substitute_test.sh` link the SDK archives that
repository vendors, per object, needing no engine at all.

⚠ **A Karma build needs the network on first configure**, like GL4ES and SDL2. To build offline
or to test an unpushed change to the recovered sources, point
`FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP` at a local checkout.

## Conventions / load-bearing facts
- **One native stack** (ufront 2.34). SDL 1.2 is retired; SDL2 + `OpenGLDrv` + `GL4ESDrv` + Pixomatic
  all live in **one** binary and the render device is picked at startup with
  `-GL4ESRENDERER` / `-OPENGLRENDERER` / `-SOFTWARERENDERER` / `-NULLRENDERER`. The old "hard backend
  split, one binary per build dir" rule is gone with the presets that implemented it.
- **Drivers are selected by `.ini`, not compiled in** (`ViewportManager`/`GameRenderDevice`/
  `AudioDevice`/`NetworkDevice`). The web build's one engine-specific addition is `WebRTCDrv`.
- **Engine differences are DATA, not code** (the project-wide rule for the multi-engine future).

## Do NOT
- **Do NOT** recreate the in-tree content symlinks (`Maps`, `Textures`, `System/*.int`, … →
  `../ut2004-assets`). They were native-runtime conveniences in the old repo and are intentionally
  absent here; content is reached via `UT_ASSETS_DIR` (wasm) or a runtime dir (native).
- **Do NOT** commit build trees (`build-*/`) or game assets — both are gitignored.
- **Do NOT** add a blanket `*.a`/`*.lib`/`*.dll` ignore — vendored flex/DirectX/Pixomatic blobs are
  intentionally tracked (see `.gitignore`).
- **Do NOT** try to fix the recovered Karma sources here. They are not in this repository; the
  change belongs in `10-p/metoolkit-karma-decomp`, where the generator and its gates live, and
  every fix there goes into the GENERATOR rather than into generated output.
- **Do NOT** make engine source changes for the relocation beyond the `UT_ASSETS_DIR`
  parameterization — that would be re-architecture, not Phase 0. Stop and re-scope.
