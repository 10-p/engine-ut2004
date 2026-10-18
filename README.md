## What?

Unreal Tournament 2004 (Unreal Engine 2.5) source with modifications to make it run on modern systems and in the browser.
Requires assets from a retail UT2004 install. Design notes, every preset and the Karma physics story: [BUILD.md](BUILD.md).

## Changes from original source

* SDL2 windowing/client driver, one native binary with desktop GL, GL4ES and Pixomatic (software) renderers.
* Karma physics from the recovered, portable sources ([10-p/metoolkit-karma-decomp](https://github.com/10-p/metoolkit-karma-decomp)).
* OpenAL Soft audio.
* GCC/MinGW support; 32-bit by default, 64-bit (x86_64) Linux and Windows builds too.
* A WebAssembly build with a WebRTC net driver (WebRTCDrv) for the browser.

## Building

Builds are 32-bit by default and use the CMake presets at the **repo root** (not `Source/`).
The first configure of each preset needs network access: SDL2, OpenAL Soft, GL4ES and Karma are fetched.

### Linux x86
1. Install git, cmake (>= 3.21), make, python3, gcc and g++ with 32-bit (multilib) support, and 32-bit X11/GL/audio development libraries (SDL2 is built from source and silently drops any backend whose library is missing).
   * Debian/Ubuntu: see the package list in [BUILD.md](BUILD.md#1-prerequisites).
   * Arch/Manjaro: enable `[multilib]`, then
     ```
     sudo pacman -S git cmake make python gcc lib32-glibc lib32-gcc-libs lib32-mesa lib32-libx11 lib32-libxext lib32-libxrandr lib32-libxcursor lib32-libxi lib32-libxinerama lib32-libxss lib32-libxkbcommon lib32-alsa-lib lib32-libpulse
     ```
2. Build:
   ```
   cmake --preset native && cmake --build --preset native -j$(nproc)
   ```
   It uses your default `gcc`/`g++` (verified with 16.2 on Arch and 15.2 on Ubuntu 26.04; CI builds with 13). To pick another, add `-DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...` to the first command.
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
3. The result is `build-wasm-perf/Source/SDLLaunch/` (`SDLLaunch.{js,wasm,data}` + `meta.json`). It is not playable on its own: the frontend repo serves it.

## Running

1. Install the retail UT2004 game files.
2. Copy the files you built (see each platform above) to `UT2004/System`.
3. Run `System/ut2004-pixo.bin` (Linux) or `System/ut2004-pixo.exe` (Windows). Pick a renderer with `-OPENGLRENDERER`, `-GL4ESRENDERER` or `-SOFTWARERENDERER`.

Use real copies of the game folders, not symlinks: the engine does not find packages through them (`Can't find file for package ...`).
Headless check: `-SOFTWARERENDERER` under `Xvfb` (also under wine); `System/UT2004.log` should reach `UT2K4MainMenu`.

## Note

Unreal Engine, Unreal Tournament 2004 and any related trademarks or copyrights are owned by Epic Games. This repository is not affiliated with or endorsed by Epic Games.
Do not use for commercial purposes.
