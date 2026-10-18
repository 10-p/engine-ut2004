#=============================================================================
# SDLLaunch - the game: the native Linux binary, the Windows exe, the Android libUT2004.so and the web
# loader (SDLLaunch.{js,wasm,data}). Links all engine modules with --whole-archive to preserve the
# AUTO_INITIALIZE_REGISTRANTS_* global constructors.
#=============================================================================

project(SDLLaunch CXX)

#-----------------------------------------------------------------------------
# Source files (mac_lame_ass_cd_check.cpp is macOS-only and not built)
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
# PUBLIC include dirs propagate via target_link_libraries; SDLLaunch also needs some
# PRIVATE includes (from Epic's SDLLaunch/Src/makefile lines 61-64, minus the Editor's).
#-----------------------------------------------------------------------------

# Include dirs: render-driver headers only for the drivers built, so no TU sees two different
# GL/gl.h sets (Mesa's and GL4ES's).
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
endif()

#-----------------------------------------------------------------------------
# Static link packages
# All engine modules are linked with --whole-archive so the linker keeps the
# AUTO_INITIALIZE_REGISTRANTS_* global constructors. Order from Epic's
# SDLLaunch/Src/makefile lines 44+57. The Editor and UTV2004 are not linked.
#
# Both GL render devices link into the same binary (their GL headers are PRIVATE, so they never meet in
# a translation unit) and the player picks one at startup with -OPENGLRENDERER / -GL4ESRENDERER.
#-----------------------------------------------------------------------------

if(EMSCRIPTEN)
  # The web stack: SDL2 + GL4ES + WebRTC (no OpenGLDrv: no desktop GL in a browser).
  set(LINK_PACKAGES
    Engine Core Fire IpDrv WebRTCDrv UWeb XGame XInterface
    GL4ESDrv ALAudio Onslaught
    zlib ogg vorbis
  )
  if(UT_WITH_PROXYNET)
    list(APPEND LINK_PACKAGES ProxyNetDrv)
  endif()
else()
  # Native/Windows: every render driver this configure enabled links in; the choice is made at runtime.
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

list(APPEND LINK_PACKAGES SDL2Drv)

# Tell SDLLaunch.cpp which render drivers are linked in, so its includes / registrants /
# dead-code refs match the link set exactly. An absent UT_RENDER_* macro evaluates to 0 via #if.
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

# Activate SDLLaunch.cpp's Pixomatic wiring (registrant, GLoadedPixoDrv, the
# -SOFTWARERENDERER -> PixoDrv.PixoRenderDevice switch + ApplySoftwareRendererConfig).
if(UT_HAS_PIXODRV)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PIXOMATIC=1)
  # pixomatic_linux.a carries MSVC-style COMDAT float-literal symbols (_real@<hex>) duplicated across its
  # object files; GNU ld doesn't merge COMDAT constants, so allow the (identical) duplicates -- ld keeps first.
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--allow-multiple-definition)
  # Pixomatic JITs into the malloc'd heap, which the kernel maps NX: SDLLaunch.cpp's main() sets the
  # READ_IMPLIES_EXEC personality and re-execs when the resolved render device is Pixomatic, which makes
  # the brk heap executable (an executable-stack marker would not).

  # Windows: stage the vendored pixomatic.dll beside the exe. It is a load-time import, not a lazy one
  # (Thirdparty/Pixomatic/Lib/pixomatic.lib is a pure import library: every member is `pixomatic.dll`),
  # so without the DLL the process does not start at all. It is vendored
  # (Thirdparty/Pixomatic/Bin/pixomatic.dll, PE32 i386) so an artifact is self-contained outside a game
  # install. There is no 64-bit build of it and no source, which is why the top-level CMakeLists refuses
  # USE_PIXOMATIC at 64-bit.
  if(WIN32 OR MINGW)
    set(_UT_PIXO_DLL "${CMAKE_SOURCE_DIR}/Thirdparty/Pixomatic/Bin/pixomatic.dll")
    if(NOT EXISTS "${_UT_PIXO_DLL}")
      message(FATAL_ERROR
        "USE_PIXOMATIC=ON on Windows needs the vendored ${_UT_PIXO_DLL}. pixomatic.lib is an import "
        "library, so the DLL is a LOAD-TIME dependency: without it the built .exe will not start at "
        "all. Vendor the DLL or configure with -DUSE_PIXOMATIC=OFF.")
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_UT_PIXO_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/pixomatic.dll"
      COMMENT "Staging vendored pixomatic.dll beside the executable (load-time import)")
  endif()
endif()

target_link_libraries(${PROJECT_NAME}
  -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive
)
if(EMSCRIPTEN)
  # openal-soft (root CMakeLists) instead of Emscripten's -lopenal. Not whole-archive: its backends are
  # a static table, not self-registering objects, so ordinary archive resolution is right.
  target_link_libraries(${PROJECT_NAME} OpenAL)
endif()

#-----------------------------------------------------------------------------
# System libraries and linker flags
#-----------------------------------------------------------------------------

if(EMSCRIPTEN)
  # Optimization/diagnostics mode: dev/debug (default) vs max performance (UT_WEB_PERF=ON).
  if(UT_WEB_PERF)
    # Max performance: optimize at link (binaryen wasm-opt -O3), no assertions, no debug/source maps.
    # -g0 ensures no DWARF reaches the linker (DWARF forces "limited binaryen optimizations").
    target_link_options(${PROJECT_NAME} PRIVATE
      -O3
      -g0
      # --emit-symbol-map writes a side file (SDLLaunch.js.symbols); the wasm itself is unchanged. The
      # shipped wasm plus this map resolves a production crash offset to a function name offline, with no
      # name section and no DWARF on the wire (DWARF would force "limited binaryen optimizations").
      # The map names every private engine symbol, so it must never reach the published tree.
      --emit-symbol-map
      -sASSERTIONS=0
      -sSTACK_OVERFLOW_CHECK=0
      # Exception catching is on in the shipping build: openal-soft reports every AL error by throwing and
      # catching it internally, and with catching off the first AL error aborts the game. Only objects
      # compiled with -fexceptions (openal-soft's, see the root CMakeLists) carry landing pads; the engine's
      # own code is compiled -fignore-exceptions and gains no invoke_ calls.
      -sNO_DISABLE_EXCEPTION_CATCHING
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

  # UT_WEB_ASSERTIONS: assertions + symbols on top of the perf build, the configuration the E2E
  # exercises. -sSAFE_HEAP is not an option: it is incompatible with -sWASMFS and fires inside
  # Emscripten's own wasmfs::FileTable constructor.
  #
  # Applied after the perf/debug branch: emcc takes the last flag and the perf branch sets
  # -sASSERTIONS=0 / -g0. CMAKE_EXE_LINKER_FLAGS cannot do this: CMake places it before
  # target_link_options, so the target's own flags would win.
  option(UT_WEB_ASSERTIONS "Emscripten: keep runtime assertions + DWARF in the perf build (diagnostic)" OFF)
  if(UT_WEB_ASSERTIONS)
    message(STATUS "SDLLaunch: DIAGNOSTIC build - assertions + stack checks + DWARF on top of -O3")
    target_link_options(${PROJECT_NAME} PRIVATE
      -g
      -sASSERTIONS=2
      -sSTACK_OVERFLOW_CHECK=2
        # FOutputDeviceAnsiError::Serialize throws on every appError/Critical; with catching on, the
        # engine unwinds into its own handler and prints the error instead of the runtime aborting
        # with "Exception thrown, but exception catching is not enabled".
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
    # PThread: the host needs the worker list to hand the FS sync channel to the thread that runs main().
    # Runtime methods are not on Module unless exported, so without it the host would silently no-op and
    # the engine would die on its first asset read.
    "-sEXPORTED_RUNTIME_METHODS=['callMain','stringToUTF8','UTF8ToString','HEAPU8','stackSave','stackAlloc','stackRestore','lengthBytesUTF8','PThread']"
    -sINITIAL_MEMORY=268435456
    -sFORCE_FILESYSTEM
    # WasmFS + the JS-implemented FS backend. Under -sWASMFS a C open() never routes through the JS FS,
    # so the host's file access goes through a WasmFS JSImpl backend whose file ops are implemented in JS
    # (vendor/glue/wasmfs-shim.js forwards them to the host-injected Module.UE.fs.backend). Asset bytes
    # stay in JS typed arrays instead of inside the 2 GB wasm32 linear memory.
    #
    # -sFORCE_FILESYSTEM (above) is required: WasmFS omits the JS FS.* API otherwise, and the host
    # glue uses it for user data, mods and the file browser.
    #
    # The EXPORTED_FUNCTIONS entries let the host stand the backend up entirely from JS
    # (wasmfs_create_jsimpl_backend + wasmfs_create_file + the shim's two registrars), so this block plus
    # the vendored shim is the whole per-engine cost.
    -sWASMFS
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_close','_ue_exec_command','_webrtc_deferred_open','_wasmfs_create_jsimpl_backend','_wasmfs_create_file','_ufront_wasmfs_register','_ufront_wasmfs_set_pending']"
    # The worker-side bootstrap in wasmfs-shim.js hangs off a __postset, and a postset is only emitted
    # if its $symbol is referenced. Nothing references it (it is pure side effect), so without this it
    # would be dead-stripped silently.
    "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$ueWorkerBoot']"
    # Run the engine on a worker (threaded only). Synchronous per-file restore from OPFS needs
    # FileSystemSyncAccessHandle, which browsers expose only on a worker, and a thread blocking on
    # Atomics.wait cannot do so on the browser main thread.
    #
    # -sOFFSCREENCANVASES_TO_PTHREAD transfers #canvas to that worker, which is what makes the direct
    # emscripten_webgl_create_context in SDL2Viewport.cpp legal there. SDL2 keeps window + input; only
    # GL-context creation and the frame present are taken off SDL (its GL path is EGL, and Emscripten
    # proxies EGL to a main thread that cannot see the transferred canvas: Emscripten #24792).
    # openal-soft mixes on its own pthread and hands the AudioContext work to the main thread itself
    # (MAIN_THREAD_EM_ASM in its web backend).
    -pthread
    -sPROXY_TO_PTHREAD
    -sOFFSCREENCANVAS_SUPPORT
    # Quoted: CMake treats an unquoted '#' as a comment, which would truncate this to
    # "OFFSCREENCANVASES_TO_PTHREAD=" and fail the link with "string index out of range".
    "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"
    # main() itself is one of these; IpDrv's host-resolve thread is not started on the web (UnIpDrv.h).
    # openal-soft adds two (its mixer thread and the event thread every context starts). A thread created
    # with the pool empty starts only after the main thread has spun up a worker for it, a start-up delay
    # the audio ring would absorb as an underrun.
    -sPTHREAD_POOL_SIZE=4
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/vendor/glue/wasmfs-shim.js"
    # No -lopenal: openal-soft (the OpenAL target, linked above) replaces Emscripten's libopenal.js, and
    # linking both would put two definitions of every al* symbol in one module.
    # No host page is baked: the engine emits a bare loader (SDLLaunch.{js,wasm,data},
    # loaderStyle=global-module, SUFFIX .js below); the host page (the frontend repository) pre-defines the
    # global `Module`, injects SDLLaunch.js, and injects the heavy glue. Only the thin shims in vendor/glue
    # are baked; the net shim's $ueNet rides in via each forwarder's __deps (the wasm imports the
    # webrtc_* symbols), so no DEFAULT_LIBRARY_FUNCS_TO_INCLUDE force-include is needed.
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/vendor/glue/webrtc-shim.js"
  )
  # Game assets live out of tree. UT_ASSETS_DIR points at the asset dir holding System/ (the web link
  # embeds System/Build.ini and System/Packages.md5 from it). Override with -DUT_ASSETS_DIR=/path.
  set(UT_ASSETS_DIR "/home/ion/ut2004-assets" CACHE PATH
      "Directory holding UT2004 game assets (System/*.ini are preloaded into the WASM build)")
  # Fail at configure, not minutes later at the link, when the tree is not there (a fresh machine).
  foreach(_f System/Build.ini System/Packages.md5)
    if(NOT EXISTS "${UT_ASSETS_DIR}/${_f}")
      message(FATAL_ERROR "UT_ASSETS_DIR: ${UT_ASSETS_DIR}/${_f} not found. "
                          "Configure with -DUT_ASSETS_DIR=<UT2004 folder> (the one holding System/).")
    endif()
  endforeach()
  # Preload the boot files into the wasm's .data (~30 KB, needed during appInit). The two config
  # templates come from the tracked web set in this repo (Engine/Config/web), not from the asset dir:
  #   * a game install's own System/*.ini is a native config (D3DDrv/WinDrv, desktop resolutions) and
  #     carries none of the web settings, notably the [WebRTCDrv.WebRTCNetDriver] section;
  #   * AllowDownloads is a per-class config property, so with no section it is never set (=> false)
  #     and every WebRTC transfer is refused with "Downloading 'x' not allowed".
  # Engine/Config/*.ini (no subdir) is the native set. appInit derives UT2004.ini/User.ini from these
  # two; the frontend's asset manifest excludes those derived names, so an install copy cannot shadow
  # them. Build.ini/Packages.md5 come from the asset dir (install metadata, not config).
  set(PRELOAD_FLAGS
    "--preload-file ${CMAKE_SOURCE_DIR}/Engine/Config/web/Default.ini@/System/Default.ini"
    "--preload-file ${CMAKE_SOURCE_DIR}/Engine/Config/web/DefUser.ini@/System/DefUser.ini"
    "--preload-file ${UT_ASSETS_DIR}/System/Build.ini@/System/Build.ini"
    "--preload-file ${UT_ASSETS_DIR}/System/Packages.md5@/System/Packages.md5"
  )
  # The preloaded inis are inputs to the link step: without LINK_DEPENDS, editing Engine/Config/web/*
  # would leave a stale .data baked (cmake sees no source change and skips the relink).
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/Engine/Config/web/Default.ini"
    "${CMAKE_SOURCE_DIR}/Engine/Config/web/DefUser.ini")
  # The --js-library shims are link inputs too, and CMake does not infer that from a link option:
  # without this, editing a shim would leave a stale .js baked while the build reported success.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/vendor/glue/wasmfs-shim.js"
    "${CMAKE_SOURCE_DIR}/vendor/glue/webrtc-shim.js")
  # ProxyNetDrv's own thin shim (the optional proxynet ABI module), a separate --js-library: the net shim
  # stays identical across every engine repo, and with UT_WITH_PROXYNET=OFF the wasm has no ue_proxynet_*
  # imports at all. Its $ueProxyNet rides in via each forwarder's __deps.
  if(UT_WITH_PROXYNET)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UT_WITH_PROXYNET=1)
    target_link_options(${PROJECT_NAME} PRIVATE
      "SHELL:--js-library ${CMAKE_SOURCE_DIR}/vendor/glue/proxynet-shim.js")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
      "${CMAKE_SOURCE_DIR}/vendor/glue/proxynet-shim.js")
  endif()
  string(REPLACE ";" " " PRELOAD_FLAGS_STR "${PRELOAD_FLAGS}")
  set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " ${PRELOAD_FLAGS_STR}")
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".js")  # a bare loader, no shell page

  # WebGL client-side vertex-array cap. Emscripten hardcodes MAX_TEMP_BUFFER_SIZE=2MB for its
  # client-side vertex array emulation (FULL_ES2). A UT2004 static-mesh batch can exceed it (several MB)
  # -> getTempVertexBuffer() indexes an undefined ring bucket -> "Cannot read properties of undefined".
  # Raise the cap to 16MB post-link so the whole batch fits one bucket.
  #
  # The pattern tolerates both emitted formats: Emscripten minifies at -O3
  # ("MAX_TEMP_BUFFER_SIZE:2097152") and pretty-prints at -O0 ("MAX_TEMP_BUFFER_SIZE: 2097152").
  # `[0-9][0-9]*` requires at least one digit, so a zero-width match (which would insert instead of
  # replace and break the .js) is impossible; [[:space:]]* absorbs the space.
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND sed -i "s/MAX_TEMP_BUFFER_SIZE:[[:space:]]*[0-9][0-9]*/MAX_TEMP_BUFFER_SIZE:16777216/" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDLLaunch.js"
    # ...and verify it: requiring the object-literal comma right after the value rejects a corrupt
    # "16777216 2097152," form, which a bare "is 16777216 present?" check would accept.
    COMMAND sh -c "grep -qE 'MAX_TEMP_BUFFER_SIZE:[[:space:]]*16777216[[:space:]]*,' \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDLLaunch.js\" || { echo 'ERROR: MAX_TEMP_BUFFER_SIZE patch did not apply cleanly - SDLLaunch.js may be syntactically broken'; exit 1; }"
    COMMENT "Raise WebGL temp vertex buffer cap 2MB->16MB (UT2004 batches exceed it; DM-AChina crash)"
    VERBATIM
  )

  # Emit the artifact descriptor meta.json after the .js is finalized above, so its sha256 is of the
  # served file. The frontend's build validates it before assembling (schema + abiVersion === glue
  # ABI_VERSION + checksums + host-supported loaderStyle). abiVersion is read from the vendored ABI
  # header (single source); provenance/toolchain are best-effort.
  file(STRINGS ${CMAKE_SOURCE_DIR}/vendor/glue/ue_abi.h _ue_abi_line REGEX "define[ \t]+UE_ABI_VERSION")
  string(REGEX MATCH "([0-9]+)" UE_ABI_VERSION "${_ue_abi_line}")
  execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE META_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(TIMESTAMP META_BUILT_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
  # The toolchain file knows where Emscripten is (a preset build has only $EMSDK set, not emcc on PATH).
  set(META_EMCC "unknown")
  if(EXISTS "${EMSCRIPTEN_ROOT_PATH}/emscripten-version.txt")
    file(READ "${EMSCRIPTEN_ROOT_PATH}/emscripten-version.txt" _emcc_ver)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" META_EMCC "${_emcc_ver}")
  endif()
  # Every fetched library as repo@branch|tag, from the cache variables the fetch itself read.
  include(${CMAKE_SOURCE_DIR}/cmake/ArtifactDeps.cmake)
  set(META_DEPS)
  ufront_artifact_dep(META_DEPS gl4es GL4ES "${GL4ES_GIT_REPOSITORY}" "${GL4ES_GIT_TAG}")
  if(USE_KARMA_DECOMP)
    ufront_artifact_dep(META_DEPS karma MetoolkitKarmaDecomp "${KARMA_DECOMP_GIT_REPOSITORY}" "${KARMA_DECOMP_GIT_TAG}")
  endif()
  ufront_artifact_dep(META_DEPS openal-soft OpenALSoft "${OPENALSOFT_GIT_REPOSITORY}" "${OPENALSOFT_GIT_TAG}")
  find_program(NODE_EXECUTABLE node REQUIRED)
  # The emitter is an input to the artifact, and CMake cannot infer that from a POST_BUILD COMMAND:
  # LINK_DEPENDS forces the relink that re-runs POST_BUILD when it changes.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs
      --out=$<TARGET_FILE_DIR:${PROJECT_NAME}>
      --target=ut2004 --abi=${UE_ABI_VERSION}
      --gitSha=${META_GIT_SHA} --builtAt=${META_BUILT_AT} --emcc=${META_EMCC}
      ${META_DEPS}
    COMMENT "Emitting artifact meta.json (step 1.5)"
    VERBATIM
  )
elseif(ANDROID)
  # Android native: SDL2 (shared, via the SDL2::SDL2-static alias) + Android system libs.
  # bionic folds pthread/m into libc; add the GLES/EGL/log/android libs SDL and the driver need.
  # libopenal.so (openal-soft) is not linked here: the engine overrides global operator new ->
  # appMalloc (GMalloc), which is not initialized until appInit(), and a DT_NEEDED link loads openal at
  # process start, so its static-init allocations would bind to the engine's operator new before GMalloc
  # exists. ALAudio dlopens "libopenal.so" at audio-init time instead (after appInit), as on the desktop;
  # AGP still packages it (openal-soft is a SHARED target).
  target_link_libraries(${PROJECT_NAME} SDL2::SDL2-static GLESv2 EGL log android dl)
  # -export-dynamic keeps the reflection/class-loading symbols and SDL_main exported for
  # SDLActivity's dlsym. No -m32 / no -rpath,. on Android.
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-export-dynamic)
else()
  # Native: link SDL and the system libraries. SDL comes after all static archives for correct symbol
  # resolution.
  if(MINGW)
    # Windows: no libdl (module loading is Win32, and BUILD_STATIC means there is nothing to load) and no
    # -rpath. winpthread is linked statically so the exe needs no extra DLL. Order matters: -lmingw32 and
    # SDL2main precede SDL2 (SDL.h renames `main` to `SDL_main`, and SDL2main's WinMain calls it).
    target_link_libraries(${PROJECT_NAME} -lmingw32 SDL2::SDL2main SDL2::SDL2-static
      -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
      opengl32 ws2_32 winmm ole32 oleaut32 imm32 version setupapi gdi32 user32 shell32 advapi32 m)
    # --export-all-symbols is the PE spelling of -export-dynamic, and it is what makes the package
    # registrants (GLoaded<Package>) visible to appGetDllExport's GetProcAddress lookup.
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols)
  else()
    target_link_libraries(${PROJECT_NAME} SDL2::SDL2-static dl pthread m)
  endif()

  # -export-dynamic: needed for the engine's reflection/class-loading system.
  # -rpath,.: allows finding vendored .so files in the same directory at runtime.
  # -m32 is gated on UT_PTR_BYTES: a 64-bit build's objects are x86-64.
  if(NOT MINGW)
    if(UT_PTR_BYTES EQUAL 4)
      target_link_options(${PROJECT_NAME} PRIVATE -m32)
    endif()
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-export-dynamic -Wl,-rpath,.)
  endif()
  # The bundled openal-soft (root CMakeLists, EAX on) beside the binary, as `libopenal.so`, the name
  # ALAudio dlopens (AL_DLL), resolved through the rpath '.' above from the System/ cwd. Copied, never
  # linked (see the Android note above for why DT_NEEDED would crash before appInit). A run tree that
  # takes the binary takes this file with it, or the game falls back to the system libopenal.
  if(UT_LINUX_OPENALSOFT)
    add_dependencies(${PROJECT_NAME} OpenAL)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:OpenAL> $<TARGET_FILE_DIR:${PROJECT_NAME}>/libopenal.so
      COMMENT "Copying libopenal.so (openal-soft, EAX) beside the binary; ALAudio dlopens it")
  endif()
endif()

#-----------------------------------------------------------------------------
# Output name: what the binary contains beyond the base set (karma variant and/or pixo), so the plain
# build is `ut2004`. Emscripten keeps the default SDLLaunch name (.js suffix); the web harness depends
# on it.
#-----------------------------------------------------------------------------
if(ANDROID)
  # libUT2004.so -- must equal GameActivity.getLibraries()'s last entry (SDLActivity loads it and
  # dlsym's SDL_main). Force the "lib" prefix (the root leaves the default prefix on Android).
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME UT2004 PREFIX "lib" SUFFIX ".so")
elseif(NOT EMSCRIPTEN)
  set(UT_BIN_NAME "ut2004")
  # The name says which physics: the default (recovered Karma) is unsuffixed because it is what ships,
  # and each deviation names itself (-nokarma, -legacykarma), so the arms of an A/B run (downloaded side
  # by side from one CI run) can be told apart.
  if(BUILD_KARMA_REF)
    set(UT_BIN_NAME "${UT_BIN_NAME}-legacykarma")
  elseif(NOT USE_KARMA_DECOMP)
    set(UT_BIN_NAME "${UT_BIN_NAME}-nokarma")
  endif()
  if(UT_HAS_PIXODRV)
    set(UT_BIN_NAME "${UT_BIN_NAME}-pixo")
  endif()
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${UT_BIN_NAME}")
  message(STATUS "SDLLaunch output name: ${UT_BIN_NAME}")
endif()

#-----------------------------------------------------------------------------
# Each preset builds into its own directory (build-<preset>/), so builds never overwrite each other's
# output. The POST_BUILD steps above and below patch the web loader, emit meta.json and stage the
# runtime DLLs/.so beside the binary.

#-----------------------------------------------------------------------------
# Windows: ship the OpenAL Soft implementation DLL next to the exe.
#
# ALAudio is DYNAMIC_BIND: it LoadLibrary's "OpenAL32.dll" at run time, so this dependency does not show
# in the exe's import table. Without the DLL the game runs without sound.
#-----------------------------------------------------------------------------
if(MINGW)
  include(${CMAKE_SOURCE_DIR}/cmake/OpenALMingw.cmake)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENAL_MINGW_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${OPENAL_MINGW_RUNTIME_DLL_NAME}"
    COMMENT "Copying ${OPENAL_MINGW_RUNTIME_DLL_NAME} (OpenAL Soft) next to the executable")
endif()
