project(PixoDrv CXX)

# PixoDrv: the Pixomatic software-rasterizer render device (UPixoRenderDevice), selected by the
# -SOFTWARERENDERER launch switch. PixoRenderDevice.cpp uploads the software framebuffer to a GL texture
# and presents it through GL4ES on the SDL2 GL window.
# The driver source is open engine code; the closed Pixomatic core library and its headers are vendored at
# Thirdparty/Pixomatic/{Inc,Lib,Bin} (a prebuilt with no source, tracked on purpose; see .gitignore).
# pixomatic_linux.a is byte-identical to Epic's v3186 drop (Pixomatic shipped only through 3186). For a
# source release that must not bundle the proprietary library, remove Thirdparty/Pixomatic and point
# PIXOMATIC_DIR at an external copy: -DPIXOMATIC_DIR=/path/to/drop (expects <PIXOMATIC_DIR>/Pixomatic/{Inc,Lib}).
set(PIXOMATIC_DIR "${CMAKE_SOURCE_DIR}/Thirdparty" CACHE PATH "Root holding Pixomatic/{Inc,Lib} (default: in-tree vendored copy)")
set(PIXOMATIC_INC "${PIXOMATIC_DIR}/Pixomatic/Inc")
# The drop ships both a Linux .a and the original Windows pixomatic.lib (the import library of
# pixomatic.dll, plain C with stdcall-decorated symbols, `_PixoFoo@40`), which MinGW links directly.
if(MINGW)
  set(PIXOMATIC_LIB "${PIXOMATIC_DIR}/Pixomatic/Lib/pixomatic.lib")
else()
  set(PIXOMATIC_LIB "${PIXOMATIC_DIR}/Pixomatic/Lib/pixomatic_linux.a")
endif()
if(NOT EXISTS "${PIXOMATIC_LIB}")
  message(FATAL_ERROR "USE_PIXOMATIC: ${PIXOMATIC_LIB} missing. Restore the vendored Thirdparty/Pixomatic/ drop, or set -DPIXOMATIC_DIR=/path/to/drop (containing Pixomatic/{Inc,Lib}).")
endif()

set(SRC_FILES
  "Src/PixoDrv.cpp"
  "Src/PixoRenderDevice.cpp"
  "Src/PixoRenderInterface.cpp"
  "Src/PixoLighting.cpp"
  "Src/PixoMaterialState.cpp"
  "Src/PixoResource.cpp"
  "Src/utils_surface.c"
  "Src/PixoCodeExec.c"   # RWX JIT code-buffer allocator (overrides the closed lib's)
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

# GPackage=PixoDrv per the makefile; __LINUX_X86__ (already global) selects the SDL Linux path in PixoDrv.h.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src ${PIXOMATIC_INC})
target_compile_definitions(${PROJECT_NAME} PRIVATE GPackage=PixoDrv)
# UT_PIXODRV_O0: pin this directory (and only this directory) to -O0 while the engine keeps -O2, a
# containment and measurement aid for a PixoDrv defect that only exists at -O2 ("is this an
# optimiser-only fault?"). The same last-wins mechanism as the Karma pin in the top-level CMakeLists.
# Off by default.
option(UT_PIXODRV_O0 "Compile Source/PixoDrv at -O0 (measurement aid / fallback containment; the engine stays -O2)" OFF)
if(UT_PIXODRV_O0)
  target_compile_options(${PROJECT_NAME} PRIVATE -O0)
  message(STATUS "PixoDrv: pinned to -O0 (UT_PIXODRV_O0; engine keeps -O2)")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine ${PIXOMATIC_LIB})

# SDL2 + GL4ES present path (texture upload + fullscreen quad). Needs the gl4es headers + targets (the
# same ones GL4ESDrv links against).
target_include_directories(${PROJECT_NAME} PRIVATE ${gl4es_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static gl4es::GL)
