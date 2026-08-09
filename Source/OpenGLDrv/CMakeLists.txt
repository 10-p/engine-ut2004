#=============================================================================
# OpenGLDrv - OpenGL render device
#
# Cross-platform OpenGL renderer with Linux-specific GL headers.
#=============================================================================

project(OpenGLDrv CXX)

set(SRC_FILES
  "Src/OpenGLDrv.cpp"
  "Src/OpenGLMaterialState.cpp"
  "Src/OpenGLRenderDevice.cpp"
  "Src/OpenGLRenderInterface.cpp"
  "Src/OpenGLResource.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc/linuxGL
)

# From OpenGLDrv/Src/makefile: -DGPackage=OpenGLDrv (no GPackage prefix for this module)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=OpenGLDrv
)

# SDL2 supplies the GL context functions (ufront 2.34: the SDL 1.2 branch is gone with SDL 1.2). Note
# what is NOT here: any GL library. This driver binds every entry point through its own pointer table
# (GL_PROC -> SDL_GL_GetProcAddress), which is exactly why it can share a binary with GL4ESDrv — there
# are no duplicate GL symbols to collide, and neither driver's GL headers are visible to the other
# (both include sets are PRIVATE).
if(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine SDL2::SDL2-static)
endif()
