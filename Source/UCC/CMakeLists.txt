#=============================================================================
# UCC - the native command-line tool (dedicated server / commandlets): ucc-bin on Linux, ucc.exe on Windows.
# No ucc on the web or Android.
#
# The native build is a single static binary (no .so packages), so this static-links the full game
# module set with --whole-archive to preserve the AUTO_INITIALIZE_REGISTRANTS_* global constructors (as
# SDLLaunch does). It is headless: no render driver, no SDL viewport, no ALAudio (ucc opens no window and
# runs -nosound).
#
# No Editor CMake target is built, so UCC_NO_EDITOR compiles out the lone editor registrant in UCC.cpp:
# ucc runs the dedicated server and the non-editor commandlets; the `make` / editor commandlets (which
# need the Editor module) are unavailable.
#=============================================================================

project(UCC CXX)

set(SRC_FILES Src/UCC.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Retail parity: the binary is `ucc-bin` (classic Linux UT ships a `ucc` wrapper script over ucc-bin), without
# the `.bin` suffix the Linux game binaries take, and `ucc.exe` on Windows, the name of the game's own UCC.exe.
if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ucc)
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ucc-bin SUFFIX "")
endif()

# UCC.cpp includes headers from many modules under __STATIC_LINK. Each module exports its Inc/ dir PUBLIC,
# so linking them propagates those includes; UnRender.h etc. live under Engine/Src, added explicitly (as
# SDLLaunch does).
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  ${CMAKE_SOURCE_DIR}/Source/Engine/Src
)

# No Editor target; ALAudio is omitted, so WITH_SPEEX has no effect here.
target_compile_definitions(${PROJECT_NAME} PRIVATE UCC_NO_EDITOR=1 WITH_SPEEX=0)

# Headless static link set (SDLLaunch's minus the client-only packages: render driver, SDL viewport,
# ALAudio). --whole-archive keeps every module's package-registrant global ctor.
set(LINK_PACKAGES Engine Core Fire IpDrv UWeb XGame XInterface Onslaught zlib ogg vorbis)

if(MINGW)
  # A console program; the C++ runtime and winpthread are linked statically by the root options, so it imports
  # system DLLs only (ws2_32: IpDrv's WinSock).
  target_link_libraries(${PROJECT_NAME}
    -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive
    ws2_32 winmm ole32 shell32 advapi32 m
  )
else()
  target_link_libraries(${PROJECT_NAME}
    -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive
    dl pthread m
  )

  # -m32 (32-bit i386, like the rest of the native build), -export-dynamic for the engine's reflection /
  # class-loading, -rpath,. so any vendored .so next to the binary resolves at runtime. The -m32 is gated on
  # UE_PTR_BYTES: a 64-bit build's objects are x86-64.
  if(UE_PTR_BYTES EQUAL 4)
    target_link_options(${PROJECT_NAME} PRIVATE -m32)
  endif()
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-export-dynamic -Wl,-rpath,.)
endif()
