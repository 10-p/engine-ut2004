#=============================================================================
# OpenGLDrv - OpenGL render device
#
# The desktop-GL renderer for native Linux and Windows, on SDL2.
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

# SDL2 supplies the GL context functions. No GL library is linked: the driver binds every entry point
# through its own pointer table (GL_PROC -> SDL_GL_GetProcAddress), which is why it can share a binary
# with GL4ESDrv (no duplicate GL symbols, and each driver's GL headers are PRIVATE).
if(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine SDL2::SDL2-static)
endif()
