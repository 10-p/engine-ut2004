#=============================================================================
# Speex - Vendored C speech codec library
#=============================================================================

project(Speex C)

set(SRC_FILES
  Src/libspeex/nb_celp.c
  Src/libspeex/sb_celp.c
  Src/libspeex/lpc.c
  Src/libspeex/ltp.c
  Src/libspeex/lsp.c
  Src/libspeex/quant_lsp.c
  Src/libspeex/lsp_tables_nb.c
  Src/libspeex/gain_table.c
  Src/libspeex/gain_table_lbr.c
  Src/libspeex/cb_search.c
  Src/libspeex/filters.c
  Src/libspeex/bits.c
  Src/libspeex/modes.c
  Src/libspeex/vq.c
  Src/libspeex/high_lsp_tables.c
  Src/libspeex/vbr.c
  Src/libspeex/hexc_table.c
  Src/libspeex/exc_5_256_table.c
  Src/libspeex/exc_5_64_table.c
  Src/libspeex/exc_8_128_table.c
  Src/libspeex/exc_10_32_table.c
  Src/libspeex/exc_10_16_table.c
  Src/libspeex/exc_20_32_table.c
  Src/libspeex/hexc_10_32_table.c
  Src/libspeex/misc.c
  Src/libspeex/speex_header.c
  Src/libspeex/speex_callbacks.c
  Src/libspeex/math_approx.c
  Src/libspeex/stereo.c
  Src/libspeex/preprocess.c
  Src/libspeex/smallft.c
  Src/libspeex/lbr_48k_tables.c
  Src/libspeex/jitter.c
  Src/libspeex/mdf.c
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src/libspeex
)

# EPIC_48K enables the 48kbps narrowband mode of Epic's ALAudio voice encoder (compiled out: WITH_SPEEX=0)
target_compile_definitions(${PROJECT_NAME} PRIVATE EPIC_48K=1)
