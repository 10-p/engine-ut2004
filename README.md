## What?

Unreal Tournament 2004 (Unreal Engine 2.5) source with modifications to make it run on modern systems and in the browser.
Requires assets from a retail UT2004 install. Every preset, option and driver: [BUILD.md](BUILD.md).

## Changes from the original source

* WebAssembly build: the whole engine runs on a browser worker, with WasmFS file access through the host page, WebGL through GL4ES and an `ue_exec_command` console export.
* SDL2 client driver (`SDL2Drv`): the one viewport on every target (window, GL context, keyboard, mouse, pads, Android touch); in the menus a pad's left stick moves the cursor, its fire controls and A click, and A (with no control under the cursor), B and the D-pad are Enter, Escape and focus moves.
* GL4ES renderer (`GL4ESDrv`): OpenGLDrv over gl4es, on OpenGL ES 2 / WebGL 1, or WebGL 2 with `WebGLVersion=2`.
* OpenGL renderer: the stock `OpenGLDrv` on SDL2, in the same binary as the other renderers.
* Software renderer: Pixomatic (`PixoDrv`) on 32-bit x86 Linux and Windows, presented through GL.
* Null renderer: `Engine.NullRenderDevice` selectable on every build with `-NULLRENDERER`.
* WebRTC net driver (`WebRTCDrv`): browser-to-browser multiplayer over WebRTC data channels, with LAN-style discovery in the server browser.
* Proxy net driver (`ProxyNetDrv`): a browser client joins a native server through a WebSocket-to-UDP game proxy.
* OpenAL Soft audio on every target, including the web (AudioWorklet) and optional HRTF.
* Karma physics from the recovered, portable sources ([10-p/metoolkit-karma-decomp](https://github.com/10-p/metoolkit-karma-decomp)) on every target.
* 64-bit and ARM support: x86-64, arm64 and armv7 builds.
* Windows build: MinGW-w64 cross builds, 32-bit (with Pixomatic) and 64-bit.
* UCC on every desktop build: `ucc-bin` (Linux) and `ucc.exe` (Windows), 32- and 64-bit, beside the game: the dedicated server and the commandlets.
* Android build: a Gradle/NDK project for Android 7.0 and later, with touch controls (hidden while a real pad is used, back on a touch) and a suspended game in the background (no tick, the OpenAL device paused); the game folder is copied into the app's storage from a folder chosen with Android's folder picker, or read in place.
* GCC/Clang portability: builds with current GCC, Clang (Emscripten, NDK) and MinGW.
* Save games: the stock `SAVEGAME`/`LOADGAME`, saved in the game folder's `Save` directory (on the web, persisted by the host).
* Languages: the stock `[Engine.Engine] Language` key, also settable from the command line with `-ini:`.

The full list, with the medium and small changes, is in the frontend's change inventory
(`<repo>/../unreal-frontend-arch-private/docs/14-change-inventory.md`).

## Building

The CMake presets are at the **repository root**. Builds are 32-bit by default. The first configure of each preset
needs network access: SDL2, OpenAL Soft, GL4ES and Karma are fetched.

### Linux x86
1. Install git, cmake (>= 3.21), make, python3, gcc and g++ with 32-bit (multilib) support, and 32-bit X11/GL/audio development libraries (SDL2 is built from source and silently drops any backend whose library is missing).
   * Debian/Ubuntu: see the package list in [BUILD.md](BUILD.md#prerequisites).
   * Arch/Manjaro: enable `[multilib]`, then
     ```
     sudo pacman -S git cmake make python gcc lib32-glibc lib32-gcc-libs lib32-mesa lib32-libx11 lib32-libxext lib32-libxrandr lib32-libxcursor lib32-libxi lib32-libxinerama lib32-libxss lib32-libxkbcommon lib32-alsa-lib lib32-libpulse
     ```
2. Build:
   ```
   cmake --preset native && cmake --build --preset native -j$(nproc)
   ```
   It uses your default `gcc`/`g++`; to pick another, add `-DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...` to the first command.
3. The result is `build-native/Source/SDLLaunch/ut2004-pixo.bin` plus `libopenal.so`, which must stay next to it.

**64-bit (x86_64):** use the `native64` preset instead of `native`, with the regular 64-bit libraries in place of the i386/lib32 ones. The result is `build-native64/Source/SDLLaunch/ut2004.bin`; it has no software renderer (Pixomatic is 32-bit only).

### Windows x86 (cross-compiled from Linux)
1. Install the MinGW-w64 i686 toolchain with the posix thread model: `g++-mingw-w64-i686-posix` (Debian/Ubuntu) or `mingw-w64-gcc` (Arch).
2. Build:
   ```
   cmake --preset windows && cmake --build --preset windows -j$(nproc)
   ```
3. The result is `build-windows/Source/SDLLaunch/ut2004-pixo.exe` plus `pixomatic.dll` and `OpenAL32.dll`, which must stay next to it.

**64-bit (x86_64):** use the `windows64` preset with the x86_64 MinGW toolchain (`g++-mingw-w64-x86-64-posix` on Debian/Ubuntu). The result is `build-windows64/Source/SDLLaunch/ut2004.exe` plus `OpenAL32.dll`; like `native64` it has no software renderer.

### Windows x86 (MSYS2 on Windows)
1. Install MSYS2 and open the `MINGW32` prompt.
2. Install dependencies: `pacman -S git make python mingw-w64-i686-toolchain mingw-w64-i686-cmake`
3. Build with the same `windows` preset as above.

### Web (WebAssembly)
1. Install [emsdk](https://github.com/emscripten-core/emsdk) 5.0.7 and load it: `./emsdk install 5.0.7 && ./emsdk activate 5.0.7 && source ./emsdk_env.sh`
2. Build (the link embeds two files from the game's `System/` folder):
   ```
   cmake --preset wasm-perf -DUE_ASSETS_DIR=<UT2004 folder> && cmake --build --preset wasm-perf -j$(nproc)
   ```
3. The result is `build-wasm-perf/Source/SDLLaunch/` (`SDLLaunch.{js,wasm,data}` + `meta.json`). It is not playable on its own: the frontend repository serves it.

## Running

1. Install the retail UT2004 game files.
2. Copy the files you built (see each platform above) to `UT2004/System`.
3. Run `System/ut2004-pixo.bin` (Linux) or `System/ut2004-pixo.exe` (Windows). Pick a renderer with `-OPENGLRENDERER`, `-GL4ESRENDERER` or `-SOFTWARERENDERER`.

The engine reads the game's folders through the paths in the ini of the `System` folder it runs from; the web link reads two files through `UE_ASSETS_DIR`.
Headless check: `-SOFTWARERENDERER` under `Xvfb` (also under wine); `System/UT2004.log` should reach `UT2K4MainMenu`.

## Credits

* Epic Games — the Unreal Tournament 2004 v3369 source, with its Linux port by Loki Software (Daniel Vogel) and Ryan C. Gordon, which the native and web builds stand on.
* RAD Game Tools — Pixomatic (the software renderer; closed library shipped with the game).
* MathEngine — Karma, recovered as portable source in [10-p/metoolkit-karma-decomp](https://github.com/10-p/metoolkit-karma-decomp).
* ptitSeb — [gl4es](https://github.com/ptitSeb/gl4es) (fork [10-p/gl4es](https://github.com/10-p/gl4es)), on Ryan Hileman's glshim.
* [OpenAL Soft](https://github.com/kcat/openal-soft) (Chris Robinson, kcat; LGPL), [SDL2](https://www.libsdl.org/), Xiph.Org (Ogg, Vorbis), Speex (Jean-Marc Valin), zlib (Jean-loup Gailly, Mark Adler), [Emscripten](https://emscripten.org/).
* [fgsfdsfgs/UE1](https://github.com/fgsfdsfgs/UE1) (fgsfds) — the CMake pattern and the SDL key map this port adapted.
* [Andiweli/UT99-Android](https://github.com/Andiweli/UT99-Android) — the Android bridge and touch overlay this port adapted.
* [alexstrout/foxWSFix-UT99](https://github.com/alexstrout/foxWSFix-UT99) — reference for the weapon field-of-view behaviour.
* [OldUnreal](https://www.oldunreal.com/) — the 3374 server build used to verify networking.

## Note

Unreal Engine, Unreal Tournament 2004 and any related trademarks or copyrights are owned by Epic Games. This repository is not affiliated with or endorsed by Epic Games.
Do not use for commercial purposes.
