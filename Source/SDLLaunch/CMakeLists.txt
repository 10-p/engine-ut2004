#=============================================================================
# SDLLaunch - Linux game executable
#
# Links all engine modules into a single binary using --whole-archive
# to preserve AUTO_INITIALIZE_REGISTRANTS_* global constructors.
#=============================================================================

project(SDLLaunch CXX)

#-----------------------------------------------------------------------------
# Source files (Linux only -- mac_lame_ass_cd_check.cpp is macOS-only)
#-----------------------------------------------------------------------------

set(SRC_FILES
  Src/SDLLaunch.cpp
)

if(ANDROID)
  # Android: SDLActivity loads the game as a shared library (libUT2004.so) and JNI-calls SDL_main.
  add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
else()
  add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

#-----------------------------------------------------------------------------
# Include directories
# SDLLaunch.cpp includes headers from many modules when __STATIC_LINK is defined.
# PUBLIC include dirs propagate via target_link_libraries, but SDLLaunch also
# needs some additional PRIVATE includes (from SDLLaunch/Src/makefile lines 61-64,
# minus Editor dirs which are excluded per D-10).
#-----------------------------------------------------------------------------

# Include dirs. The Emscripten path is kept exactly as before; native paths add render-driver
# headers only for the driver actually built -- a TU must never see two different GL/gl.h sets
# (the Mesa-vs-GL4ES header clash behind the legacy/gl4es hard split).
if(EMSCRIPTEN)
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Source/Engine/Src
    ${CMAKE_SOURCE_DIR}/Source/OpenGLDrv/Inc/linuxGL
    ${CMAKE_SOURCE_DIR}/Source/GL4ESDrv/Inc
    ${gl4es_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/Thirdparty/OpenAL/Inc
  )
else()
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Source/Engine/Src
    ${CMAKE_SOURCE_DIR}/Thirdparty/OpenAL/Inc
  )
  if(UT_HAS_OPENGLDRV)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source/OpenGLDrv/Inc/linuxGL)
  endif()
  if(UT_HAS_GL4ESDRV)
    target_include_directories(${PROJECT_NAME} PRIVATE
      ${CMAKE_SOURCE_DIR}/Source/GL4ESDrv/Inc
      ${gl4es_SOURCE_DIR}/include
    )
  endif()
  # SDL 1.2 include path for OpenGLDrv.h / SDLDrv.h (which include SDL.h on non-Windows).
  if(USE_SDL12)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL12_INCLUDE_DIR})
  endif()
endif()

#-----------------------------------------------------------------------------
# Static link packages
# All engine modules linked with --whole-archive to prevent the linker from
# stripping AUTO_INITIALIZE_REGISTRANTS_* global constructors (Pitfall 1).
# Order derived from SDLLaunch/Src/makefile lines 44+57.
# Editor is excluded per D-10. UTV2004 was never in the link list.
#
# Only one SDL viewport driver is linked per binary because SDL 1.2 and SDL 2
# share symbol names with incompatible internal state.
# Switch with: cmake -B build -DUSE_SDL12=ON
#-----------------------------------------------------------------------------

if(EMSCRIPTEN)
  # Emscripten forced stack: SDL2 + GL4ES + WebRTC (no OpenGLDrv, no SDLDrv, no GLES3Drv).
  set(LINK_PACKAGES
    Engine Core Fire IpDrv WebRTCDrv UWeb XGame XInterface
    GL4ESDrv ALAudio Onslaught
    zlib ogg vorbis
  )
else()
  # Native: the render driver depends on the backend (HARD SPLIT). Only the built driver links.
  set(LINK_PACKAGES Engine Core Fire IpDrv UWeb XGame XInterface)
  if(UT_HAS_OPENGLDRV)
    list(APPEND LINK_PACKAGES OpenGLDrv)
  endif()
  if(UT_HAS_GL4ESDRV)
    list(APPEND LINK_PACKAGES GL4ESDrv)
  endif()
  if(UT_HAS_PIXODRV)
    list(APPEND LINK_PACKAGES PixoDrv)
  endif()
  list(APPEND LINK_PACKAGES ALAudio Onslaught zlib ogg vorbis)
endif()

# WITH_SPEEX=0: ALAudio voice capture/VoIP disabled (no Speex dependency)
target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_SPEEX=0)

if(EMSCRIPTEN)
  # On Emscripten, always use SDL2Drv (no SDL 1.2 option)
  list(APPEND LINK_PACKAGES SDL2Drv)
elseif(USE_SDL12)
  list(APPEND LINK_PACKAGES SDLDrv)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL12=1)
else()
  list(APPEND LINK_PACKAGES SDL2Drv)
endif()

# Tell SDLLaunch.cpp which render drivers are actually linked in, so its includes / registrants /
# dead-code refs match the link set exactly. An absent UT_RENDER_* macro evaluates to 0 via #if,
# so the unused driver (and notably the never-built GLES3Drv) compiles out cleanly.
if(EMSCRIPTEN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE UT_RENDER_GL4ESDRV=1)
else()
  if(UT_HAS_OPENGLDRV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UT_RENDER_OPENGLDRV=1)
  endif()
  if(UT_HAS_GL4ESDRV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UT_RENDER_GL4ESDRV=1)
  endif()
endif()

# ufront 2.12 Part B: activate SDLLaunch.cpp's Pixomatic wiring (registrant, GLoadedPixoDrv, the
# -SOFTWARERENDERER -> PixoDrv.PixoRenderDevice switch + ApplySoftwareRendererConfig).
if(UT_HAS_PIXODRV)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PIXOMATIC=1)
  # pixomatic_linux.a carries MSVC-style COMDAT float-literal symbols (_real@<hex>) duplicated across its
  # object files; GNU ld doesn't merge COMDAT constants, so allow the (identical) duplicates -- ld keeps first.
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--allow-multiple-definition)
  # NB: Pixomatic JITs into the malloc'd heap, which a modern kernel marks NX. The fix is NOT an executable-stack
  # marker (a modern kernel no longer turns that into READ_IMPLIES_EXEC) — SDLLaunch.cpp's main() sets the
  # READ_IMPLIES_EXEC personality + re-execs when -SOFTWARERENDERER is active, which makes the brk heap executable.
endif()

target_link_libraries(${PROJECT_NAME}
  -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive
)

#-----------------------------------------------------------------------------
# System libraries and linker flags
#-----------------------------------------------------------------------------

if(EMSCRIPTEN)
  # Optimization/diagnostics mode: dev/debug (default) vs max performance (UT_WEB_PERF=ON).
  if(UT_WEB_PERF)
    # Max performance: optimize at link (binaryen wasm-opt -O3), no assertions, no debug/source maps.
    # -g0 ensures no DWARF reaches the linker (DWARF forces "limited binaryen optimizations").
    # Exception CATCHING disabled (no slow invoke_* trampolines around every throwing call). Safe on
    # this target: __UNIX__ makes guard/unguard no-ops, the engine has zero `throw` statements, and
    # fatal errors abort -- the only compiled try/catch is the error reporter, never hit in normal play.
    target_link_options(${PROJECT_NAME} PRIVATE
      -O3
      -g0
      -sASSERTIONS=0
      -sSTACK_OVERFLOW_CHECK=0
      -sDISABLE_EXCEPTION_CATCHING=1
    )
  else()
    # Dev/debug: assertions, stack-overflow checks, DWARF + source maps, exception catching ON.
    target_link_options(${PROJECT_NAME} PRIVATE
      -g
      -gsource-map
      --source-map-base ./
      -sASSERTIONS=2
      -sSTACK_OVERFLOW_CHECK=2
      -sNO_DISABLE_EXCEPTION_CATCHING
    )
  endif()
  # Common Emscripten linker flags (both modes): Asyncify for sync-to-async, SDL2/OpenAL ports, WebGL.
  target_link_options(${PROJECT_NAME} PRIVATE
    -sSTACK_SIZE=2097152
    -sASYNCIFY
    -sASYNCIFY_STACK_SIZE=262144
    "-sASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']"
    -sFULL_ES2=1
    -sUSE_SDL=2
    -sALLOW_MEMORY_GROWTH=1
    # PThread (2.13.2): the host needs the worker list to hand the FS sync channel to the thread that
    # runs main(). Runtime methods are NOT on Module unless exported, so omitting it makes the host
    # silently no-op and the engine dies on its first asset read with no clue as to why.
    "-sEXPORTED_RUNTIME_METHODS=['callMain','stringToUTF8','UTF8ToString','HEAPU8','stackSave','stackAlloc','stackRestore','lengthBytesUTF8','PThread']"
    -sINITIAL_MEMORY=268435456
    -sFORCE_FILESYSTEM
    # ufront 2.13.1 — WasmFS + the JS-implemented FS backend.
    #
    # -sWASMFS moves the VFS into wasm, which BREAKS FetchFS's old seam: it intercepted file access by
    # monkey-patching the JS FS.open (legal only because the legacy filesystem implements
    # __syscall_openat in JS), and under WasmFS a C open() never routes through it — probed 2026-07-14,
    # the patch installs and never fires. The replacement is a WasmFS JSImpl backend whose file ops are
    # implemented in JS (vendor/glue/wasmfs-shim.js forwards them to the host-injected
    # Module.UE.fs.backend), which is both the on-demand hook and the way to keep asset bytes in JS
    # typed arrays instead of a std::vector inside linear memory. That distinction is load-bearing:
    # measured, the engine itself sits flat at 256 MB of linear memory while assets reach 1.74 GB, so
    # putting residency inside a 2 GB wasm32 address space would cap and fragment it for no gain.
    #
    # -sFORCE_FILESYSTEM (above) stays REQUIRED — WasmFS omits the JS FS.* API otherwise, and the host
    # glue still uses it for user data, mods and the file browser.
    #
    # The EXPORTED_FUNCTIONS entries let the HOST stand the backend up entirely from JS
    # (wasmfs_create_jsimpl_backend + wasmfs_create_file + the shim's two registrars), so this block plus
    # the vendored shim is the whole per-engine cost — no C++ in any engine repo.
    -sWASMFS
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_close','_ue_exec_command','_webrtc_deferred_open','_wasmfs_create_jsimpl_backend','_wasmfs_create_file','_ufront_wasmfs_register','_ufront_wasmfs_set_pending']"
    # The worker-side bootstrap in wasmfs-shim.js hangs off a __postset, and a postset is only EMITTED
    # if its $symbol is referenced. Nothing references it (it is pure side effect), so without this it
    # is dead-stripped SILENTLY — no error, no warning, and the engine dies on its first asset read.
    "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$ueWorkerBoot']"
    # ufront 2.13.2 — run the engine on a WORKER (threaded-only, decision D2).
    #
    # WHY: synchronous per-file restore-on-read from OPFS (2.13.4, the actual point of stage 2.13) needs
    # FileSystemSyncAccessHandle, which browsers expose ONLY on a worker; and a thread blocking on
    # Atomics.wait cannot do so on the browser main thread. So main() has to leave the main thread.
    #
    # -sOFFSCREENCANVASES_TO_PTHREAD transfers #canvas to that worker, which is what makes the direct
    # emscripten_webgl_create_context in SDL2Viewport.cpp legal there. SDL2 keeps window + input; only
    # GL-context creation and the frame present are taken off SDL (its GL path is EGL, and Emscripten
    # proxies EGL to a main thread that can no longer see the transferred canvas — Emscripten #24792).
    #
    # -sASYNCIFY deliberately STAYS this step: dropping it is 2.13.3, after the worker move is proven.
    # Audio needs nothing here — Emscripten's OpenAL library is already __proxy:'sync' throughout, so
    # its Web Audio calls run on the main thread by themselves.
    -pthread
    -sPROXY_TO_PTHREAD
    -sOFFSCREENCANVAS_SUPPORT
    # QUOTED: CMake treats an unquoted '#' as a comment, which silently truncates this to
    # "OFFSCREENCANVASES_TO_PTHREAD=" and fails the link with "string index out of range".
    "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"
    # main() itself is one of these. The engine spawns no other threads on the web (IpDrv's host-resolve
    # pthread_create is __EMSCRIPTEN__-guarded off, see UnIpDrv.h), so a small pool avoids paying for
    # workers nothing will use.
    -sPTHREAD_POOL_SIZE=2
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/vendor/glue/wasmfs-shim.js"
    -lopenal
    # 1.6a (ufront): the host PAGE is NO LONGER baked here. The engine emits a BARE LOADER —
    # SDLLaunch.{js,wasm,data}, loaderStyle=global-module — and the host page lives in the public
    # monorepo (packages/frontend/index.html; → Vite+Vue in 1.6b), which pre-defines the global
    # `Module` then injects SDLLaunch.js. So `--shell-file web/shell.html` is dropped and SUFFIX is
    # .js (below, was .html). The entire engine-side `web/` dir (the old baked glue + shell.html + the
    # served-file copies) was REMOVED in ufront 1.9 — the shell/UI/glue all live in the public monorepo.
    # Heavy glue (FetchFS + the four IDB stores) is NOT baked — it's @ufront/glue ESM, injected by the
    # host (steps 1.4a/1.4b). Only the thin webrtc_* shim (vendor/glue/webrtc-shim.js) is baked; its
    # $ueNet rides in via each forwarder's __deps (the wasm imports the webrtc_* symbols), so no
    # DEFAULT_LIBRARY_FUNCS_TO_INCLUDE force-include is needed.
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/vendor/glue/webrtc-shim.js"
  )
  # Game assets live OUT of tree (this is the source-only engine repo). UT_ASSETS_DIR points at the
  # canonical asset dir holding System/, Maps/, Textures/, ... Override with -DUT_ASSETS_DIR=/path.
  # (Parameterized in step 0.1 of the ufront migration; was the now-relocated
  # ${CMAKE_SOURCE_DIR}/../ut2004-assets.)
  set(UT_ASSETS_DIR "/home/ion/ut2004-assets" CACHE PATH
      "Directory holding UT2004 game assets (System/*.ini are preloaded into the WASM build)")
  # Preload essential INI files into MEMFS (~30KB, needed during appInit).
  # The two BOOT CONFIG templates come from the TRACKED, WEB-SPECIFIC set in this repo
  # (Engine/Config/web), not from the out-of-tree asset dir. Rationale:
  #   * a game install's own System/*.ini is a NATIVE config (D3DDrv/WinDrv, desktop resolutions) and
  #     carries none of the web-only settings — notably the [WebRTCDrv.WebRTCNetDriver] section;
  #   * AllowDownloads is a per-CLASS config property, so with no section it is never set (=> false)
  #     and every WebRTC transfer is refused with "Downloading 'x' not allowed";
  #   * sourcing config from an untracked asset tree meant rebuilding the asset image silently
  #     reverted the web config. Engine/Config/web is version-controlled, so that cannot happen.
  # Engine/Config/*.ini (no subdir) remains the platform-agnostic / native reference set.
  # appInit derives UT2004.ini/User.ini from these two; those derived names are excluded from the
  # ufront asset manifest so a stale install copy can never shadow them.
  # Build.ini/Packages.md5 stay asset-side (install metadata, not config).
  set(PRELOAD_FLAGS
    "--preload-file ${CMAKE_SOURCE_DIR}/Engine/Config/web/Default.ini@/System/Default.ini"
    "--preload-file ${CMAKE_SOURCE_DIR}/Engine/Config/web/DefUser.ini@/System/DefUser.ini"
    "--preload-file ${UT_ASSETS_DIR}/System/Build.ini@/System/Build.ini"
    "--preload-file ${UT_ASSETS_DIR}/System/Packages.md5@/System/Packages.md5"
  )
  # The preloaded inis are INPUTS to the link step: without LINK_DEPENDS, editing Engine/Config/web/*
  # leaves a stale .data baked (cmake sees no source change and skips the relink) — a silent trap.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/Engine/Config/web/Default.ini"
    "${CMAKE_SOURCE_DIR}/Engine/Config/web/DefUser.ini")
  # The --js-library shims are link INPUTS too, and CMake does not infer that from a link option:
  # editing a shim left a STALE .js baked while the build reported success, so the change appeared to
  # have no effect (2.13.2: the worker never received the target descriptor and silently cached
  # nothing). Same class of trap as the preloaded inis above, same fix.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/vendor/glue/wasmfs-shim.js"
    "${CMAKE_SOURCE_DIR}/vendor/glue/webrtc-shim.js")
  string(REPLACE ";" " " PRELOAD_FLAGS_STR "${PRELOAD_FLAGS}")
  set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " ${PRELOAD_FLAGS_STR}")
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".js")  # 1.6a: bare loader (was ".html" via --shell-file)

  # WebGL client-side vertex-array cap. Emscripten hardcodes MAX_TEMP_BUFFER_SIZE=2MB for its
  # client-side vertex array emulation (FULL_ES2). UT2004 static-mesh batches exceed it (e.g.
  # DM-AChina = 4.66MB) -> getTempVertexBuffer() indexes an undefined ring bucket
  # (tempVertexBuffers1[idx]) -> "Cannot read properties of undefined" -> render crash / black
  # screen. GL4ES passes the batch as a client-side array (legal in GLES2; WebGL only emulates it
  # up to the cap). Raise the cap to 16MB post-link so the whole batch fits one bucket.
  #
  # ufront 2.19 — the pattern must tolerate BOTH emitted formats. Emscripten minifies at -O3
  # ("MAX_TEMP_BUFFER_SIZE:2097152") but pretty-prints at -O0 ("MAX_TEMP_BUFFER_SIZE: 2097152"), and the
  # old regex `MAX_TEMP_BUFFER_SIZE:[0-9]*` matched ZERO digits before that space — so instead of REPLACING
  # it INSERTED, producing `MAX_TEMP_BUFFER_SIZE:16777216 2097152` and a hard "Uncaught SyntaxError:
  # Unexpected number" that took the entire debug build down before main() ever ran. `[0-9][0-9]*` requires
  # at least one digit, making the zero-width match impossible; [[:space:]]* absorbs the space.
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND sed -i "s/MAX_TEMP_BUFFER_SIZE:[[:space:]]*[0-9][0-9]*/MAX_TEMP_BUFFER_SIZE:16777216/" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDLLaunch.js"
    # …and VERIFY it, because the failure above was SILENT: the build reported success and shipped a .js no
    # browser can parse. Requiring the object-literal comma right after the value rejects the corrupt
    # "16777216 2097152," form, which a bare "is 16777216 present?" check would happily accept.
    COMMAND sh -c "grep -qE 'MAX_TEMP_BUFFER_SIZE:[[:space:]]*16777216[[:space:]]*,' \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDLLaunch.js\" || { echo 'ERROR: MAX_TEMP_BUFFER_SIZE patch did not apply cleanly - SDLLaunch.js may be syntactically broken'; exit 1; }"
    COMMENT "Raise WebGL temp vertex buffer cap 2MB->16MB (UT2004 batches exceed it; DM-AChina crash)"
    VERBATIM
  )

  # Emit the artifact descriptor meta.json (docs/05 §4, step 1.5) AFTER the .js is finalized above,
  # so its sha256 is of the served file. packages/build validates it before assembling serve-web
  # (schema + abiVersion === glue ABI_VERSION + checksums + host-supported loaderStyle). abiVersion is
  # read from the vendored frozen ABI header (single source); provenance/toolchain are best-effort.
  file(STRINGS ${CMAKE_SOURCE_DIR}/vendor/glue/ue_abi.h _ue_abi_line REGEX "define[ \t]+UE_ABI_VERSION")
  string(REGEX MATCH "([0-9]+)" UE_ABI_VERSION "${_ue_abi_line}")
  execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE META_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(TIMESTAMP META_BUILT_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
  execute_process(COMMAND emcc --version
    OUTPUT_VARIABLE _emcc_ver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" META_EMCC "${_emcc_ver}")
  find_program(NODE_EXECUTABLE node REQUIRED)
  # ufront 2.13.2 — the emitter is an INPUT to the artifact, and CMake cannot infer that from a
  # POST_BUILD COMMAND: editing it changes nothing the build watches, so the target does not relink,
  # POST_BUILD never re-runs, and a STALE meta.json ships while the build reports success. Hit for real
  # when buildFlags was corrected. Same class of trap as the preloaded config and the vendored shims,
  # same fix — LINK_DEPENDS forces the relink that re-runs POST_BUILD.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs
      --out=$<TARGET_FILE_DIR:${PROJECT_NAME}>
      --target=ut2004 --abi=${UE_ABI_VERSION}
      --gitSha=${META_GIT_SHA} --builtAt=${META_BUILT_AT} --emcc=${META_EMCC}
    COMMENT "Emitting artifact meta.json (step 1.5)"
    VERBATIM
  )
elseif(ANDROID)
  # Android native: SDL2 (shared, via the SDL2::SDL2-static alias) + Android system libs.
  # bionic folds pthread/m into libc; add the GLES/EGL/log/android libs SDL and the driver need.
  # NOTE: libopenal.so (openal-soft) is deliberately NOT linked here. The engine overrides global
  # operator new -> appMalloc (GMalloc), which isn't initialized until appInit(); a DT_NEEDED link
  # loads openal at process start, so its static-init allocations bind to the engine's operator new
  # before GMalloc exists -> null -> crash. Instead ALAudio dlopens "libopenal.so" at audio-init time
  # (after appInit), like the desktop build. AGP still packages it (openal-soft is a SHARED target).
  target_link_libraries(${PROJECT_NAME} SDL2::SDL2-static GLESv2 EGL log android dl)
  # -export-dynamic keeps the reflection/class-loading symbols and SDL_main exported for
  # SDLActivity's dlsym. No -m32 / no -rpath,. on Android.
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-export-dynamic)
else()
  # Native: link SDL library and system libraries
  # SDL must come AFTER all static archives for correct symbol resolution
  # (Pitfall 2 in RESEARCH.md and Phase 1 fix).
  if(USE_SDL12)
    target_link_libraries(${PROJECT_NAME} ${SDL12_LIBRARY} dl pthread m)
  else()
    target_link_libraries(${PROJECT_NAME} SDL2::SDL2-static dl pthread m)
  endif()

  # -export-dynamic: needed for the engine's reflection/class-loading system.
  # -rpath,.: allows finding vendored .so files in the same directory at runtime.
  target_link_options(${PROJECT_NAME} PRIVATE -m32 -Wl,-export-dynamic -Wl,-rpath,.)
endif()

#-----------------------------------------------------------------------------
# Output name: distinct per backend so the two builds are unambiguous and can sit side by side.
# Emscripten keeps the default SDLLaunch name (+ .html suffix) -- the web harness depends on it.
#-----------------------------------------------------------------------------
if(ANDROID)
  # libUT2004.so -- must equal GameActivity.getLibraries()'s last entry (SDLActivity loads it and
  # dlsym's SDL_main). Force the "lib" prefix (the root leaves the default prefix on Android).
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME UT2004 PREFIX "lib" SUFFIX ".so")
elseif(NOT EMSCRIPTEN)
  if(UT_GFX_BACKEND STREQUAL "legacy")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ut2004-legacy)
  else()
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ut2004-gl4es)
  endif()
endif()

#-----------------------------------------------------------------------------
# Post-build: disabled. Each build target outputs to its own build directory
# (build-sdl1/, build-physics/, etc.) and runtime directories
# (runtime-nokarma/, runtime-physics/) use symlinks to the correct binary.
# This avoids builds overwriting each other's output.
#-----------------------------------------------------------------------------
