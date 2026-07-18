#=============================================================================
# libvorbis - Vendored C Vorbis audio codec library
#=============================================================================

project(vorbis C)

set(SRC_FILES
  lib/analysis.c
  lib/bitrate.c
  lib/block.c
  lib/codebook.c
  lib/envelope.c
  lib/floor0.c
  lib/floor1.c
  lib/info.c
  lib/lpc.c
  lib/lsp.c
  lib/mapping0.c
  lib/mdct.c
  lib/psy.c
  lib/registry.c
  lib/res0.c
  lib/sharedbook.c
  lib/smallft.c
  lib/synthesis.c
  lib/time0.c
  lib/vorbisfile.c
  lib/window.c
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC ogg)
