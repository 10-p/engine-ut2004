#=============================================================================
# GL4ESDrv - GL4ES (GL-to-GLES2 translation) render device
#
# A renamed fork of OpenGLDrv compiled against the GL4ES static library, which translates desktop
# GL 1.x/2.x calls to GLES 2.0 / WebGL. SDL2 only.
#=============================================================================

project(GL4ESDrv CXX)

set(SRC_FILES
  "Src/GL4ESDrv.cpp"
  "Src/GL4ESMaterialState.cpp"
  "Src/GL4ESRenderDevice.cpp"
  "Src/GL4ESRenderInterface.cpp"
  "Src/GL4ESResource.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${gl4es_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GL4ESDrv
)

# Link against the GL4ES static library (provides all GL symbols) via its namespaced alias gl4es::GL
# (defined in the top-level CMakeLists): a "::" target name can never silently degrade to the system
# -lGL. System OpenGL is not linked; GL4ES replaces it.
if(NOT TARGET gl4es::GL)
  message(FATAL_ERROR "GL4ESDrv requires the GL4ES static target (gl4es::GL), which is missing. "
                      "GL4ESDrv must only be built in the gl4es backend or for Emscripten.")
endif()
if(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine gl4es::GL)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine SDL2::SDL2-static gl4es::GL)
endif()
