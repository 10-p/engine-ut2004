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

# Link against whichever SDL is active for GL context functions.
if(USE_SDL12)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL12_INCLUDE_DIR})
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL12=1)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
elseif(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine SDL2::SDL2-static)
endif()
