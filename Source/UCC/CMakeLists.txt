#=============================================================================
# UCC - Linux command-line tool (dedicated server / commandlets)
#
# ufront 2.12 Part B (native). There is NO ucc on the web build -- this target is native-only.
#
# UT2004 native is a SINGLE STATIC binary (no .so packages), so -- unlike engine-ut99's ucc, which links
# only Engine+Core and loads the rest dynamically -- this must static-link the full game module set with
# --whole-archive to preserve the AUTO_INITIALIZE_REGISTRANTS_* global constructors (same mechanism as
# SDLLaunch, Pitfall 1). It is headless: no render driver, no SDL viewport, no ALAudio (ucc opens no
# window and runs -nosound), so those packages are intentionally omitted.
#
# Editor is EXCLUDED (D-10): no Editor CMake target is built for native, so UCC_NO_EDITOR compiles out the
# lone editor registrant in UCC.cpp. ucc still runs the dedicated server + non-editor commandlets; the
# `make` / editor commandlets are unavailable (they need the Editor module).
#=============================================================================

project(UCC CXX)

set(SRC_FILES Src/UCC.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Retail parity: the binary is `ucc-bin` (classic Linux UT ships a `ucc` wrapper script over ucc-bin).
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ucc-bin)

# UCC.cpp includes headers from many modules under __STATIC_LINK. Each module exports its Inc/ dir PUBLIC,
# so linking them propagates those includes; UnRender.h etc. live under Engine/Src, added explicitly (as
# SDLLaunch does).
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  ${CMAKE_SOURCE_DIR}/Source/Engine/Src
)

# Editor excluded (no native Editor target); ALAudio omitted so WITH_SPEEX has no effect here.
target_compile_definitions(${PROJECT_NAME} PRIVATE UCC_NO_EDITOR=1 WITH_SPEEX=0)

# Headless static link set (mirrors SDLLaunch minus the client-only packages: render driver, SDL viewport,
# ALAudio). --whole-archive keeps every module's package-registrant global ctor (Pitfall 1).
set(LINK_PACKAGES Engine Core Fire IpDrv UWeb XGame XInterface Onslaught zlib ogg vorbis)

target_link_libraries(${PROJECT_NAME}
  -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive
  dl pthread m
)

# -m32 (32-bit i386, like the rest of the native build), -export-dynamic for the engine's reflection /
# class-loading, -rpath,. so any vendored .so next to the binary resolves at runtime.
target_link_options(${PROJECT_NAME} PRIVATE -m32 -Wl,-export-dynamic -Wl,-rpath,.)
