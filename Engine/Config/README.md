# Engine/Config — reference config set

Tracked **reference copy** of the boot config the web build uses:

- `Default.ini` — engine/game defaults (render device `GL4ESDrv`, viewport `SDL2Drv`,
  net `WebRTCDrv`, audio `NOpenALDrv` for the web target).
- `DefUser.ini` — default user/input config. `appInit` derives `UT2004.ini` / `User.ini`
  from these two on first boot.

**Not consumed by the build.** The web (Emscripten) build `--preload-file`s the *live*
copies from the out-of-tree asset dir (`UT_ASSETS_DIR`, see `Source/SDLLaunch/CMakeLists.txt`),
and native builds read config from the game `System/` they run in. Platform/renderer
differences are applied in engine code (the launcher's `#ifdef __EMSCRIPTEN__` overrides),
so the shipped data stays pristine.

This folder is the canonical in-repo snapshot to sync into an install / the asset dir,
mirroring `engine-unreal1/Engine/Config` and `engine-ut99/Config/System`. Keep it in sync
when the web config changes.
