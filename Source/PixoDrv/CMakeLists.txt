project(PixoDrv CXX)

# ufront 2.12 Part B: the Pixomatic software-rasterizer render device (UPixoRenderDevice), selected by the
# -SOFTWARERENDERER launch switch. SINGLE-SOURCE: this in-tree driver builds on BOTH backends --
#   * legacy (USE_SDL12): the original SDL 1.2 video-surface present.
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
set(PIXOMATIC_LIB "${PIXOMATIC_DIR}/Pixomatic/Lib/pixomatic_linux.a")
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

if(USE_SDL12)
  # Legacy backend: SDL 1.2 present path (SDL_GetVideoSurface + SDL_Flip).
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL12_INCLUDE_DIR})
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL12=1)
else()
  # gl4es backend: SDL2 + GL4ES present path (texture upload + fullscreen quad). Needs the gl4es headers +
  # targets (the same GL4ESDrv links against).
  target_include_directories(${PROJECT_NAME} PRIVATE ${gl4es_SOURCE_DIR}/include)
  target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static gl4es::GL)
endif()
