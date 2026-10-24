# Building engine-ut2004

The engine source of the UT2004 (Unreal Engine 2.5) port: native Linux and Windows binaries, an Android
library and the WebAssembly artifact the frontend serves. Game assets live out of tree; only the web link reads
two files from them (`-DUE_ASSETS_DIR`).

## Prerequisites

Package names are Debian/Ubuntu; [README.md](README.md) has the Arch equivalents. Every native preset except
`native64`/`windows64` is 32-bit (`-m32`), which needs an i386 multiarch sysroot.

```bash
# compilers and tools (the system gcc; CMake >= 3.21)
sudo apt install gcc g++ cmake git python3

# 32-bit native presets
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
                 libibus-1.0-dev:i386 libudev-dev:i386
#   SDL2 and openal-soft are built from source and probe for each of these; a missing
#   one does not fail the configure, it drops a backend.
#   native64: the same packages without the :i386 suffix.

# web presets: Emscripten 5.0.7; source emsdk_env.sh before any wasm preset (the presets read $EMSDK)
git clone https://github.com/emscripten-core/emsdk <emsdk>
<emsdk>/emsdk install 5.0.7 && <emsdk>/emsdk activate 5.0.7

# Windows cross-builds (optional): the posix thread model
sudo apt install g++-mingw-w64-i686-posix g++-mingw-w64-x86-64-posix
```

The first configure of a build directory needs the network: SDL2, openal-soft, GL4ES and the Karma SDK +
recovered sources are FetchContent'd into `build-*/_deps/`. A branch ref is re-fetched on every configure
(`cmake --preset …`), never by a plain `cmake --build`.

## Build

Configure with the preset, then build. Each preset has its own `build-<preset>/` directory.

```bash
cmake --preset native && cmake --build --preset native -j"$(nproc)"
#   -> build-native/Source/SDLLaunch/ut2004-pixo.bin (+ libopenal.so beside it)

source <emsdk>/emsdk_env.sh
cmake --preset wasm-perf -DUE_ASSETS_DIR=<UT2004 folder> && cmake --build --preset wasm-perf -j"$(nproc)"
#   -> build-wasm-perf/Source/SDLLaunch/SDLLaunch.{js,wasm,data} + meta.json + SDLLaunch.js.symbols
```

`UE_ASSETS_DIR` is required by the web presets: it is the UT2004 folder holding `System/`, and the link embeds
`System/Build.ini` and `System/Packages.md5` from it (the two boot inis come from `Engine/Config/web`). The
configure stops early if the files are not there. The native presets read no assets at build time.

## Presets

`cmake --list-presets` lists them.

| Preset | What it builds | Output (in `build-<preset>/Source/SDLLaunch/`) |
|---|---|---|
| `native` | 32-bit Linux (the default width): SDL2Drv + OpenGLDrv + GL4ESDrv + PixoDrv in one binary, the recovered Karma | `ut2004-pixo.bin` |
| `native64` | 64-bit Linux, the same engine and Karma at LP64; no Pixomatic (32-bit only) | `ut2004.bin` |
| `native-sse` | 32-bit with SSE float math (`UE_FPMATH_SSE`), no Pixomatic: the pointer-width control of the Karma trace gate | `ut2004.bin` |
| `legacy-karma` | `native` with MathEngine's prebuilt i386 archives as the physics (`BUILD_KARMA_REF`): the A/B reference | `ut2004-legacykarma-pixo.bin` |
| `native-nokarma` | `native` without physics (`USE_KARMA_DECOMP=OFF`) | `ut2004-nokarma-pixo.bin` |
| `windows` | 32-bit Windows (MinGW-w64 cross) with Pixomatic and the recovered Karma | `ut2004-pixo.exe` + `pixomatic.dll` + `OpenAL32.dll` |
| `windows64` | 64-bit Windows (LLP64) with the recovered Karma; no Pixomatic | `ut2004.exe` + `OpenAL32.dll` |
| `windows-nokarma` | `windows` without physics | `ut2004-nokarma-pixo.exe` |
| `wasm` | the web dev build and the shipped `-debug` binary: `-O2 -g`, assertions, exception catching | `SDLLaunch.{js,wasm,data}` |
| `wasm-debug` | `-O0 -g` (GL4ES stays `-O2`): when the optimiser is the suspect | same |
| `wasm-perf` | the shipping web build: `-O3 -g0`, no assertions, a side symbol map | same |
| `wasm-nokarma` | `wasm-perf` without physics | same |
| `wasm-karmadecomp-debug`, `wasm-karmadecomp-perf` | aliases of `wasm-debug` / `wasm-perf` (`USE_KARMA_DECOMP` is already the default) | same |

The binary name says what it contains beyond the base set: `-legacykarma` or `-nokarma` for the physics, `-pixo`
for Pixomatic. The web keeps the `SDLLaunch` name; Android builds `libUT2004.so`.

Every Linux and Windows preset also builds the command-line tool, headless (no render device, viewport or audio):
`build-<preset>/Source/UCC/ucc-bin` on Linux, `ucc.exe` on Windows. It runs the dedicated server
(`./ucc-bin server "DM-Rankin?game=XGame.xDeathMatch" -port=7777`) and the commandlets but `make` and the editor
ones (no Editor target is built); it reads `UT2004.ini` and writes `ucc.log`. SIGTERM (Linux) or the console's
Ctrl-C (Windows) asks a server to exit cleanly.

## Options

| Option | Default | Effect |
|---|---|---|
| `UE_ALLOW_64BIT` | OFF | x86 Linux/Windows build x86_64 instead of `-m32` (`native64`, `windows64`). `UE_PTR_BYTES` is the width the build produces. |
| `UE_ASSETS_DIR` | (none) | web presets: the UT2004 folder holding `System/` (required). |
| `UE_WEB_PERF`, `UE_WEB_DEBUG` | OFF | the web build mode: `-O3 -g0` / `-O0 -g`; neither = `-O2 -g`. Set by the presets. |
| `UE_WEB_ASSERTIONS` | OFF | assertions + exception catching on top of the perf build (a diagnostic build). |
| `UE_WITH_PROXYNET` | ON | web: build ProxyNetDrv (joining native servers through a game proxy) and link its shim. |
| `UE_BUNDLED_OPENAL` | ON | native Linux: build openal-soft (EAX on) and put `libopenal.so` beside the binary; OFF dlopens the system library. |
| `UE_FPMATH_SSE` | OFF | 32-bit x86: `-msse2 -mfpmath=sse` (the `native-sse` preset). |
| `USE_KARMA_DECOMP` | ON | link the recovered Karma (`metoolkit_decomp::all`). |
| `BUILD_KARMA_REF` | OFF | link MathEngine's prebuilt i386 archives instead (32-bit x86 Linux only; refused elsewhere). |
| `USE_PIXOMATIC` | OFF (ON in the 32-bit native and Windows presets) | build PixoDrv (32-bit x86 only; refused at 64-bit). |
| `UE_PIXODRV_O0` | OFF | compile PixoDrv at `-O0` (a diagnostic). |
| `UE_KARMA_O2_FILES`, `UE_KARMA_O2_OPTIONS`, `UE_KARMA_PIN_WEB` | empty, `-O2`, OFF | measure the recovered Karma's `-O0` pin (below). |
| `UE_ENGINE_TU_FILES`, `UE_ENGINE_TU_OPTIONS` | empty | extra compile options for matching Engine sources (a bisect aid). |
| `KD_CENSUS` | OFF | instrument the recovered Karma with a per-function call counter (never a shipping build). |
| `GL4ES_GIT_REPOSITORY`, `GL4ES_GIT_TAG` | `10-p/gl4es`, `ufront` | the GL4ES source; stock upstream is `ptitSeb/gl4es` `v1.1.6` (wrong cubemaps). |
| `KARMA_DECOMP_GIT_TAG` | `main` | the metoolkit-karma-decomp revision. |

A local checkout replaces any fetched dependency: `-DFETCHCONTENT_SOURCE_DIR_GL4ES=…`,
`-DFETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP=…` (also how to build offline).

## Karma

Karma (MathEngine `metoolkit`) is UT2004's physics: Onslaught vehicles and ragdolls. The SDK and its recovery as
portable C live in [10-p/metoolkit-karma-decomp](https://github.com/10-p/metoolkit-karma-decomp), fetched at
configure time; this repository holds the engine-side bridge (`Source/Engine/Src/K*.cpp`, `KarmaSupport.cpp`) and
the CMake that links it. Three arms:

| | links | presets |
|---|---|---|
| `USE_KARMA_DECOMP=ON` (default) | `metoolkit_decomp::all`, the recovered sources | every preset that does not say otherwise, Android included |
| `BUILD_KARMA_REF=ON` | MathEngine's shipped i386 Linux archives (+ `KarmaGlibcCompat.c`) | `legacy-karma` |
| both OFF | nothing (`NO_KARMA=1`) | `*-nokarma` |

- `BUILD_KARMA_REF` is 32-bit x86 Linux only. Windows is refused: MinGW links MathEngine's win32 archives, but GNU
  ld drops their cross-object COMDAT relocations (`call` with displacement 0 at 42 sites in the collision core), so
  the binary would corrupt its stack in collision detection.
- The recovered C is compiled at `-O0` on native and Windows while the engine keeps `-O2`: at `-O2` GCC's
  value-range propagation folds two matrix loops of
  `MdtKea/keaCalcIworldandNonInertialForceandVhmf_vanilla.c` whose decompiled subscripts run past the inner array.
  The web builds it at `-O2`/`-O3` (clang does not draw that inference). `UE_KARMA_O2_FILES` (a regex over source
  basenames) and `UE_KARMA_O2_OPTIONS` compile matching sources with other options; `UE_KARMA_PIN_WEB` applies the
  pin to the web build.
- `-KTRACE=<path>` (requires `-FIXEDFPS=<n>`) writes one CSV row per simulated body per frame; `-KTRACECONTACTS`,
  `-KTRACEBONES`, `-KTRACEEVERY=`, `-KTRACEFRAMES=`, `-KTRACECONSOLE` refine it. The trace gates are that
  repository's `decomp/test/ut2004/`.
- A build directory configured with `USE_KARMA_DECOMP=OFF` keeps it (an existing cache wins over the default);
  the configure says `Physics: NO_KARMA` every time.

## The web build

- A bare loader: `SDLLaunch.{js,wasm,data}` (`loaderStyle=global-module`), injected by the frontend's host page,
  which also injects the browser glue. Only the thin shims in `vendor/glue/` are baked in (`net-shim.js`,
  `wasmfs-shim.js`, `proxynet-shim.js`); `vendor/glue/ue_abi.h` is the ABI (version 2) and the source of
  `meta.json`'s `abiVersion`.
- The engine runs on a worker (`-sPROXY_TO_PTHREAD`, `#canvas` transferred to it). The viewport creates the WebGL
  context itself: WebGL 1 by default, WebGL 2 with `[SDL2Drv.SDL2Client] WebGLVersion=2` or `-WEBGL=2` (falling back
  to WebGL 1 where the browser has no WebGL 2; GL4ES then runs its OpenGL ES 3 back end). Each frame yields to the
  worker's event loop until the display's next frame (`-TIMERFRAMES`: from timers, e.g. for a benchmark).
- Files go through WasmFS with a JS-implemented backend that forwards every file operation to the host.
- `meta.json` records the ABI version, the git sha, the emcc version and every fetched dependency as
  `<repo>@<branch|tag>` (`local:<path>` for a local override; `cmake/ArtifactDeps.cmake`).
- The link raises Emscripten's client-array temp buffer (`MAX_TEMP_BUFFER_SIZE`) to 16 MB and verifies the patch.
- The perf build writes `SDLLaunch.js.symbols`, a side symbol map that resolves a crash offset to a function name
  offline. It names every private engine symbol: never publish it.
- Not used: Closure (would mangle the baked JS) and LTO (could drop the `AUTO_INITIALIZE_REGISTRANTS_*`
  constructors that `--whole-archive` keeps).

## Audio

ALAudio runs on openal-soft everywhere: the `10-p/openal-soft-web` fork (`ufront/web`) is linked static into the
wasm and built shared for Android and native Linux; Windows stages OpenAL Soft's official DLL as `OpenAL32.dll`
(`cmake/OpenALMingw.cmake`). ALAudio loads `AL_DLL` first (`libopenal.so` / `OpenAL32.dll`) and falls back to the
system library (`libopenal.so.1`; the retail `DefOpenAL32.dll` on Windows). The library is dlopen'd at audio init,
never linked as DT_NEEDED. A run tree that takes the Linux binary takes `libopenal.so` with it. On Android it plays
through OpenSL ES and is built without C++20 modules (`ALSOFT_ENABLE_MODULES=OFF`) and without RTKit.

## Render devices and drivers

| Switch | Render device | Built in |
|---|---|---|
| `-GL4ESRENDERER` | `GL4ESDrv.GL4ESRenderDevice` | every build (the only GL device on the web and Android; the web ini's default) |
| `-OPENGLRENDERER` | `OpenGLDrv.OpenGLRenderDevice` | native and Windows (the native ini's default) |
| `-SOFTWARERENDERER` | `PixoDrv.PixoRenderDevice` | 32-bit native and Windows (`USE_PIXOMATIC`) |
| `-NULLRENDERER` | `Engine.NullRenderDevice` | every build |

The first `-*RENDERER` switch on the command line decides; a device the build lacks is logged (`render device <X> not
available`) and the default stays. A choice that survives `Engine->Init` is written to the ini. A device that gets no
window or GL context stops the game with `Render device <X> could not start` (after the `SDL_GL_CreateContext failed`
warning), and its switch is not written. GL4ESDrv needs an OpenGL ES context: on Windows a driver with
`WGL_EXT_create_context_es2_profile`, or `libEGL.dll` and `libGLESv2.dll` (ANGLE) beside the exe. With no switch, the launcher replaces any `ViewportManager`,
`RenderDevice` or `AudioDevice` the ini names but the build lacks: the viewport with `SDL2Drv.SDL2Client`, the
render device with OpenGLDrv then GL4ESDrv, the audio device with ALAudio (or `UseSound=False`); on the web a stock
`IpDrv.TcpNetDriver` becomes `WebRTCDrv.WebRTCNetDriver`. `-NoReplaceViewport` and `-NoReplaceRenderDevice` turn
the first two replacements off.

- `[<Device>] NumAASamples` (OpenGLDrv, GL4ESDrv; 0 = off): MSAA sample count. The native ini ships 0, the web ini
  4. On the web it decides the WebGL `antialias` flag only. An ini without the key is read through the old
  `MultisampleBuffers`/`MultisampleSamples` pair.
- DXT textures (GL4ESDrv) go to the GPU as they are where it has S3TC. Where it has not, gl4es transcodes them at
  upload to ASTC 4x4 (decoded at 8-bit precision where the GPU offers it) or ETC2, and only with neither decodes them
  to RGBA8 (`LIBGL_AVOID16BITS`). The transcode happens in the texture precache at level load (`Finished precaching
  textures in <s> seconds`).
- OpenGLDrv and GL4ESDrv share their quality settings: render targets on (`UseRenderTargets`), 16x anisotropic
  filtering clamped to the GPU's maximum, all texture units, trilinear; an ini without the device's section gets
  them as compiled defaults.
- `[<Device>] UseHardwareGamma` (OpenGLDrv, GL4ESDrv, PixoDrv; default False; `-NOGAMMA` forces it off): the
  in-game gamma reprograms the display's ramp; RestoreGamma puts back the ramp found at startup.
- Pixomatic's JIT runs code from the heap: on Linux the launcher re-execs itself with `READ_IMPLIES_EXEC` when the
  resolved render device is Pixomatic.

## Settings and commands of this port

- `-ini:<File>:[<Section>]:<Key>=<Value>` writes a config value straight after the config cache loads (`User` /
  `System` are the generic file names); one `Ini override:` log line each.
- `-UFRONTLOG` adds the port's diagnostic and repeated log lines: `[AutoFOV]` and `[AutoFOVUI]`, the 10 s
  `[NET-TRAFFIC]` traffic and replication windows and the `[WebRTC]` receive counts, the `[ENGINE-DBG]` boot steps,
  the menu-cursor handover (`SDL2: menu cursor:`), GL4ES's initialization steps, the `UseHardwareGamma=` (OpenGLDrv,
  GL4ESDrv) and `Vertex buffers:` (GL4ESDrv) lines of every mode set, OpenGLDrv's and GL4ESDrv's device information
  of every mode set after the process's first (the `GL_VENDOR`/`GL_RENDERER`/`GL_VERSION` lines, the `Device
  supports:` list, the buffer formats, anisotropy, multisampling, pixel shaders and render targets), and the EAX
  room-effect line. Status lines, warnings, errors and command answers are logged either way.
- `[ufront.AutoFOV]` (`AutoFOV`, `ReferenceAspect`, `WeaponScale`, `AutoFOVUI`, `UIScale`): aspect-correct field
  of view and HUD scale (both `Default.ini` sets document the keys).
- `[ufront.Shadows]` `CullDistance` / `FadeExponent`: the pawn-shadow range and fall-off (retail 1200 / 2.0; the
  web ini ships 4800).
- `[Engine.Engine] FrameRateCap=<fps>`: caps a game with no tick rate of its own.
- `[Engine.GameEngine] FpsLogSeconds=<s>` / `FPSLOG <s>`: an `FPS:` log line per interval.
- `WHEREAMI` / `TELEPORT <x> <y> <z> [<yaw> [<pitch>]]`: allowed in a standalone game and for a listen server's
  host; `[Engine.GameEngine] AllowNetCheats=True` (test harnesses only) opens them elsewhere.
- `FLUSHCONFIG`: write the config cache to disk now.
- `[SDL2Drv.SDL2Client] UseJoystick` (on by default): game pads, one pad (the first connected), the shared button
  map, sticks sent as rates. In play the left stick is the move stick: its deadzone is radial (0.12, rescaled from its
  edge), so every direction between the axes keeps both parts, and a tilt out of it but below
  `[SDL2Drv.SDL2Client] PadWalkTilt` (0.5 of the stick's travel; 0 never) walks, as the Walking button does: the stick
  keeps a bit of its own set in the player's `bRun` (the Walking key writes the whole byte; the stick sets its bit
  again the next update), and the moves a client sends to its server carry `bRun`. While a menu is up, any pad button or a stick leaving its rest position takes the
  cursor until the mouse moves: the GUI draws it and the system cursor is hidden. Only while the pad has the cursor
  (that press included) do its controls act on the menu; in play and while the mouse has the cursor they give their
  game keys and axes. The left stick moves the cursor, linear past the deadzone, across the viewport's width in
  `[SDL2Drv.SDL2Client] PadCursorSeconds` (3.0) at full tilt (in fullscreen, where the GUI adds up the mouse axes
  itself, times the menu mouse sensitivity). Primary fire (right trigger and bumper) clicks the left button and alt
  fire the right; A clicks the control under the pad's cursor as primary fire does, and with no control there (or
  before the pad's cursor shows) is Enter, which activates the focused control; B is Escape (back), and the D-pad the
  arrow keys, an arrow no control uses moving the focus as Tab does (down and right to the next control, up and left
  to the previous one). A key-binding page waiting for a key gets the pad's own keys.
- `-usehomedir`: user files under `~/.ut2004` instead of beside the game (the default).
- A key-less install generates its CD key (63 bits of OS entropy) and saves it to `System/cdkey`.
- `UT_NO_FAULT_HANDLER=1` (Android: `adb shell setprop debug.ut2004.nofault 1`): leave hardware faults to the OS's
  crash reporter instead of the engine's handler.

## Windows

The MinGW-w64 cross build uses `cmake/toolchain-mingw-w64.cmake` (the presets select it; its thread model is
`posix`, which the engine needs; an MSYS2 `MINGW32` prompt builds the same presets without it). `UnUnix.cpp` is the
platform layer there too (LoadLibrary/GetProcAddress with `--export-all-symbols`); the C++ runtime and winpthread
are linked statically, so the exe needs only system DLLs plus `OpenAL32.dll` (and `pixomatic.dll` on the 32-bit
preset, a load-time import). A hardware fault, `std::terminate` and `abort` print the image base and raw addresses
for `addr2line -e ut2004.exe`. `ucc.exe`, the retail UCC's name, imports system DLLs only; it reads its arguments as
`ucc-bin` does (the ones after the commandlet's name), and a SIGINT to the wine process reaches it as Ctrl-C.

## Android

`Android/` is a Gradle/NDK project (Android Gradle plugin 9.4.0, Gradle 9.6.0 through the wrapper) whose
`externalNativeBuild` points at this repository's root `CMakeLists.txt` (`targets SDLLaunch ut2004_android_bridge
OpenAL`), built for `armeabi-v7a`, `arm64-v8a` and `x86_64` in one APK (x86 Chromebooks and the emulator run the
last). It needs platform 36 (`compileSdk`; `targetSdk` is 34, `minSdk` 24), build-tools 36.0.0, NDK
28.2.13676358 and CMake 4.1.2 or newer. Ubuntu's packages install the SDK parts into `/usr/lib/android-sdk`;
`local.properties` names it and the distribution's CMake:

```bash
sudo apt install openjdk-17-jdk cmake ninja-build adb google-android-cmdline-tools-19.0-installer \
  google-android-platform-36-installer google-android-build-tools-36.0.0-installer google-android-ndk-r28c-installer
printf 'sdk.dir=/usr/lib/android-sdk\ncmake.dir=/usr\n' > Android/local.properties
```

```bash
cd Android
./gradlew :app:assembleRelease      # -> app/build/outputs/apk/release/app-release.apk (assembleDebug: debuggable)
#   -PnoKarma            no physics
#   -PkarmaDir=<path>    a local metoolkit-karma-decomp checkout
#   -Psdl2Dir=<path>     a local SDL2 checkout (or SDL2_SOURCE_DIR)
#   -Pgl4esDir=<path>    a local gl4es checkout (or GL4ES_SOURCE_DIR)
#   -Px86abi             also 32-bit x86 (an emulator control, not for shipping)
```

- Signing: every variant is signed with the key the Gradle property `ueAndroidSigning` names, a properties file kept
  outside the repository (`storeFile`, `storePassword`, `keyAlias`, `keyPassword`), set in `~/.gradle/gradle.properties`
  or as `UE_ANDROID_SIGNING`. One key signs every release, so each APK installs over the last; without the property
  the build uses the debug key.
- The Gradle daemon runs on an installed JDK 17 (`Android/gradle/gradle-daemon-jvm.properties`), whatever `java` is
  on the `PATH` or Android Studio's own JDK.
- The SDK in `/usr/lib/android-sdk` is read-only, so a part the Gradle plugin would download instead fails the build
  with its name.

In play the touch overlay (`GameActivity.java`, into `Source/SDL2Drv/Src/SDL2Viewport.cpp`) has a move stick on the
left half, drag-look on the right and FIRE, ALTFIRE, JUMP, CROUCH, NEXTWEAPON and MENU buttons. The move stick moves
in the drag's direction and tilts fully as the launcher's "Touch move stick" settings say: full speed at a drag of
12 dp from where the finger is held (4 to 60) and a dead zone of 3 dp for a resting finger (0 to 10, always below full
speed), saved with the launcher's other settings and handed to the game when it starts
(`GameActivity.EXTRA_MOVE_FULL_TILT_DP`, `EXTRA_MOVE_DEAD_ZONE_DP`); past full tilt the held point follows the finger.
It never walks.

The engine reads its game folder by path (the JNI bridge sets `UT2004_ANDROID_BASEDIR` and the cwd); the launcher
(`MainActivity`) chooses which:

- Internal storage (`/data/data/com.ut2004.android/files/UT2004`): fast, deleted with the app, no permission.
  "Choose the UT2004 folder" opens Android's folder picker, which gives the app that folder only, and "Copy to
  internal storage" copies it in (only what is new or changed: a stopped copy resumes); with no folder chosen yet it
  opens the picker first. "Copy settings and saves" writes the internal copy's `System/UT2004.ini`,
  `System/User.ini` and `Save/` back to the chosen folder.
- A UT2004 folder in place: `Android/data/com.ut2004.android/files/UT2004` on shared storage or an SD card needs no
  permission; `/sdcard/UT2004` or `UT2004` at the top of an SD card needs All files access (the storage permission
  on Android 10 and earlier), which the launcher asks for, with the copy as the alternative, only when the game is
  started from there.

A travel URL can be passed from adb:

```bash
adb shell am start -n com.ut2004.android/.MainActivity \
  --es com.ut2004.android.ARGS 'ONS-Dria?Game=Onslaught.ONSOnslaughtGame?bAutoNumBots=True?QuickStart=True?bPlayerMustBeReady=False'
```

`GameActivity` is not exported; launch `MainActivity`. GLog goes to logcat (tag `UT2004`).

## Verifying a build

```bash
# Every compiled-in device is in the native binary:
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UOpenGLRenderDevice
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UGL4ESRenderDevice
nm -C build-native/Source/SDLLaunch/ut2004-pixo.bin | grep -c UPixoRenderDevice
# The web artifact's descriptor:
python3 -c "import json;print(json.load(open('build-wasm-perf/Source/SDLLaunch/meta.json'))['abiVersion'])"
```

CI (`.github/workflows/build.yml`) builds the native, Windows, Android and web presets on every push and asserts the
physics each binary carries, the Windows imports, the packaged Android ABIs and the web descriptor.

## Input test channel

`-INPUTTEST=<file>` opens the input test channel (`Source/SDL2Drv/Src/SDL2InputTest.h`): commands appended to the file
drive a virtual SDL game controller that takes the pad's place, run console commands and watch the view and the pawn
once per input update. `tools/dev/inputtest/run.sh <run-tree> <scenario> <out-dir> [<args>]` runs the native build
under Xvfb with it and feeds it a scenario (`move.scen`: the move stick's speed against tilt and its directions on
DM-Rankin); `rate.py` gives the turn rate, the speed, the move direction and the walking share from the log.

## Layout

| Dir | What |
|---|---|
| `Source/` | the engine C++: Core, Engine, Fire, IpDrv, XGame, XInterface, UWeb, Onslaught, ALAudio, the render/viewport/net drivers, the `SDLLaunch`/`UCC` launchers (plus Epic's Windows/editor code the build does not compile) |
| `Thirdparty/` | zlib, Vorbis, Speex, the OpenAL headers, Pixomatic (the closed library, vendored on purpose), `metoolkit_dummy` (Epic's stub Karma libraries; the build does not use them) |
| `Engine/Config/` | the tracked inis: the native set and `web/`, the set the web link embeds ([Engine/Config/README.md](Engine/Config/README.md)) |
| `vendor/glue/` | the ABI header and shims the web build bakes in |
| `Android/` | the Gradle/NDK project |
| `cmake/` | CMake modules and the MinGW toolchain file |
| `tools/dev/inputtest/` | the input test channel's runner, its scenarios and `rate.py` |
