#=============================================================================
# libogg - Vendored C Ogg container format library
#=============================================================================

project(ogg C)

set(SRC_FILES
  src/bitwise.c
  src/framing.c
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
