#=============================================================================
# Fire - Procedural texture effects
#
# Fractal fire, water, ice texture generation.
#=============================================================================

project(Fire CXX)

set(SRC_FILES
  "Src/UnFractal.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageFire
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
