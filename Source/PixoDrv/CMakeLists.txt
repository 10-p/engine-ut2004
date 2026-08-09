project(PixoDrv CXX)

# ufront 2.12 Part B: the Pixomatic software-rasterizer render device (UPixoRenderDevice), selected by the
# -SOFTWARERENDERER launch switch. SINGLE-SOURCE: this in-tree driver builds on BOTH backends --
#   * (ufront 2.34) the SDL 1.2 video-surface present is gone with SDL 1.2 itself.
#   * gl4es  (SDL2):       PixoRenderDevice.cpp's SDL2 path uploads the software framebuffer to a GL texture
#                          and presents it through GL4ES (SDL2 GL window).
# The DRIVER SOURCE is in-tree + open (engine render-device code). The CLOSED Pixomatic core library + its
# headers are VENDORED in-tree at <repo>/Pixomatic/{Inc,Lib} (a prebuilt lib with no source, tracked like
# metoolkit/ and DirectX*/ -- see .gitignore), so the -full / legacy-full presets compile out of the box.
# pixomatic_linux.a is byte-identical to the original Epic v3186 drop (Pixomatic shipped only through 3186).
# For a source release that must NOT bundle the proprietary lib, strip <repo>/Pixomatic and point PIXOMATIC_DIR
# at an external copy: -DPIXOMATIC_DIR=/path/to/drop (expects <PIXOMATIC_DIR>/Pixomatic/{Inc,Lib}).
set(PIXOMATIC_DIR "${CMAKE_SOURCE_DIR}/Thirdparty" CACHE PATH "Root holding Pixomatic/{Inc,Lib} (default: in-tree vendored copy)")
set(PIXOMATIC_INC "${PIXOMATIC_DIR}/Pixomatic/Inc")
# ufront 2.34 — the drop ships BOTH a Linux .a and the original Windows pixomatic.lib. The Windows one is
# plain C with stdcall-decorated symbols (`_PixoFoo@40`), not MSVC-mangled C++, so MinGW can link it
# directly — unlike Karma's metoolkit, which is C++ and therefore unusable off MSVC (2.34 section 2.1).
if(MINGW)
  set(PIXOMATIC_LIB "${PIXOMATIC_DIR}/Pixomatic/Lib/pixomatic.lib")
else()
  set(PIXOMATIC_LIB "${PIXOMATIC_DIR}/Pixomatic/Lib/pixomatic_linux.a")
endif()
if(NOT EXISTS "${PIXOMATIC_LIB}")
  message(FATAL_ERROR "USE_PIXOMATIC: ${PIXOMATIC_LIB} missing. Restore the in-tree <repo>/Pixomatic/ vendored drop, or set -DPIXOMATIC_DIR=/path/to/drop (containing Pixomatic/{Inc,Lib}).")
endif()

set(SRC_FILES
  "Src/PixoDrv.cpp"
  "Src/PixoRenderDevice.cpp"
  "Src/PixoRenderInterface.cpp"
  "Src/PixoLighting.cpp"
  "Src/PixoMaterialState.cpp"
  "Src/PixoResource.cpp"
  "Src/utils_surface.c"
  "Src/PixoCodeExec.c"   # ufront 2.12: RWX JIT code-buffer allocator override (overrides the closed lib's)
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

# GPackage=PixoDrv per the makefile; __LINUX_X86__ (already global) selects the SDL Linux path in PixoDrv.h.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src ${PIXOMATIC_INC})
target_compile_definitions(${PROJECT_NAME} PRIVATE GPackage=PixoDrv)
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine ${PIXOMATIC_LIB})

# SDL2 + GL4ES present path (texture upload + fullscreen quad). Needs the gl4es headers + targets (the
# same ones GL4ESDrv links against).
target_include_directories(${PROJECT_NAME} PRIVATE ${gl4es_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static gl4es::GL)
