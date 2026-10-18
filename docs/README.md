# engine-ut2004 docs

- [`../README.md`](../README.md) — what this port changes from the original source, how to build and run it, credits.
- [`../BUILD.md`](../BUILD.md) — prerequisites, presets, options, Karma, the web build, drivers, the layout.
- [`../Engine/Config/README.md`](../Engine/Config/README.md) — the tracked boot inis and their sections.
- [`../CLAUDE.md`](../CLAUDE.md) — the rules for changing this repository.

## Layout

`Source/` holds the engine C++ (Epic's packages plus SDL2Drv, GL4ESDrv, WebRTCDrv, ProxyNetDrv and the launchers),
`Thirdparty/` the vendored libraries (zlib, Vorbis, Speex, the OpenAL headers, Pixomatic, `metoolkit_dummy`),
`Engine/Config/` the boot inis (native and `web/`), `Android/` the Gradle/NDK project, `vendor/glue/` the ABI header
and shims the web build bakes in, and `cmake/` the CMake modules and the MinGW toolchain file. Game content is never
in the tree: the web link reads two files through `UE_ASSETS_DIR`, and a native run reads the game folder it starts
in.

The feature list of every engine is the frontend's docs/14-change-inventory.md.
