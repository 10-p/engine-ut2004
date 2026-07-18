#=============================================================================
# zlib - Vendored C compression library
#=============================================================================

project(zlib C)

set(SRC_FILES
  adler32.c
  compress.c
  crc32.c
  deflate.c
  infblock.c
  infcodes.c
  inffast.c
  inflate.c
  inftrees.c
  infutil.c
  trees.c
  uncompr.c
  zutil.c
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
