#=============================================================================
# Engine - Core simulation module
#
# Actor system, game loop tick, physics (Karma bridge), rendering interface,
# networking base, AI framework. ~115+ source files including Karma stubs
# (guarded by #ifdef WITH_KARMA, excluded via NO_KARMA=1).
#=============================================================================

project(Engine CXX)

# Engine has ~115+ source files. Using GLOB for this legacy codebase where
# no new files will be added. K*.cpp files compile but their Karma contents
# are excluded by #ifdef WITH_KARMA guards (NO_KARMA=1 defined globally).
file(GLOB SRC_FILES "Src/*.cpp")

# xTexShader.cpp is not in the original makefile and has compile errors
# (references removed FRenderInterface methods). Exclude it.
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Src/xTexShader.cpp")

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageEngine
  ThisPackage=Engine
)

# From Engine/Src/makefile LIBS: Core, ogg, vorbis (KARMA_LIBS empty with NO_KARMA=1)
target_link_libraries(${PROJECT_NAME} PRIVATE Core zlib ogg vorbis)

# ---------------------------------------------------------------------------
# USE_KARMA_DECOMP — the RECOVERED Karma, compiled from source.
#
# WITH_KARMA=1 + metoolkit's public headers (portable C, and never what was missing); what it
# links is `metoolkit_decomp::all` rather than the gcc-3.2 x86 archives MathEngine shipped —
# `metoolkit_decomp::all` — 145 objects recovered from those archives' DWARF, plus the
# hand-written convex-hull replacement that stands in for qhull.
#
# THE SOURCES DO NOT LIVE HERE ANY MORE (ufront 2.44). They are
# https://github.com/10-p/metoolkit-karma-decomp, fetched by the top-level CMakeLists, which
# also carries the SDK, the toolchain that produces them, and the gates that judge them. This
# repository holds the engine-side bridge and nothing else.
if(USE_KARMA_DECOMP)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KARMA=1)
  # METOOLKIT_DIR is set at the top level, from the FetchContent'd
  # metoolkit-karma-decomp (ufront 2.44). It is a CACHE PATH there, so an
  # override on the command line still wins.
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${METOOLKIT_DIR}/include
    ${METOOLKIT_DIR}/include/MdtBcl
    ${METOOLKIT_DIR}/include/MdtKea
    ${METOOLKIT_DIR}/include/McdCommon
    ${METOOLKIT_DIR}/include/McdPrimitives
    ${METOOLKIT_DIR}/include/McdFrame
    ${METOOLKIT_DIR}/include/Mst
    ${METOOLKIT_DIR}/include/MeGlobals
    ${METOOLKIT_DIR}/include/MeApp
  )

  # ★ THE LIBRARY IS A FETCHED CMAKE TARGET NOW, NOT A GLOB OF THIS REPOSITORY
  # (ufront 2.44). What used to be a file(GLOB) over karma-decomp/generated/allobj
  # plus a hand-rolled add_library here is `metoolkit_decomp::all` there, which
  # owns the flags, the include paths and the ME_FORCE_CDECL define.
  #
  # THE FLAGS DID NOT CHANGE AND ARE STILL NOT NEGOTIABLE — they moved. Decompiled
  # C puns pointers through integer slots constantly, so -Wno-int-conversion /
  # -Wno-incompatible-pointer-types are load-bearing rather than sloppy, and
  # -fno-strict-aliasing is what stops the compiler miscompiling storage the
  # recovery reinterprets the way the original machine code did. They live in
  # metoolkit_decomp/CMakeLists.txt and its standalone gate validates 145/145
  # against them.
  #
  # AND THERE IS STILL NO --start-group, for the same reason as before: the
  # sixteen archives have circular dependencies, so the fetched package collects
  # every object into ONE archive where the linker sees the whole symbol set at
  # once. Its CMakeLists carries the full note.
  if(NOT TARGET metoolkit_decomp::all)
    message(FATAL_ERROR
      "USE_KARMA_DECOMP: metoolkit_decomp::all is not defined. The top-level "
      "CMakeLists FetchContent's https://github.com/10-p/metoolkit-karma-decomp; "
      "if you are offline, point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at "
      "a local checkout of it.")
  endif()
  add_library(KarmaDecomp INTERFACE)
  target_link_libraries(KarmaDecomp INTERFACE metoolkit_decomp::all)

  # KD_CENSUS — count every recovered function the REAL GAME enters. OFF by default; it changes
  # the code generation of all 145 objects and must never be a shipping build.
  #
  # WHY IT IS WORTH A CACHE VARIABLE. "This object was substituted and the trajectory did not
  # change" and "this object never executed" produce the identical line of output from every gate
  # in the recovery's standalone tier, which is what its kd_instr.c was written for.
  # The same question is sharper against a live match: a whole-library A/B that reads bit-identical
  # proves the recovery exact ON THE PATHS THE MAP EXERCISED, and says nothing whatever about the
  # rest. This is how you find out which those are.
  #
  #   cmake --preset native -B build-native-census -DKD_CENSUS=ON \
  #         -DCMAKE_EXE_LINKER_FLAGS=-no-pie
  #
  # ⚠ -no-pie IS LOAD-BEARING AND MUST GO ON THE EXECUTABLE. kd_instr.c records the RUNTIME
  # address of each function; `nm` prints link-time offsets, and in a PIE those differ by the load
  # bias — so every address resolves to nothing and the report reads "0 of 2025 functions entered",
  # which is indistinguishable from "the physics never ran". test/scene_census.sh links -no-pie for
  # the same reason. Putting it on this static library instead does NOTHING, which is how it was
  # got wrong the first time: the build succeeded, `file` still said "pie executable", and the
  # census still read zero. census_report.py now refuses a zero rather than printing one.
  #   KD_INSTR_OUT=/tmp/census.txt <binary> <map> ...
  #   python3 metoolkit-karma-decomp `decomp/tools/census_report.py /tmp/census.txt <binary>
  #
  # -finstrument-functions is applied ONLY to the recovered objects, so the engine, the shipped
  # library and libc cost nothing. kd_instr.c is a separate TU compiled WITHOUT the flag, or the
  # counter would recurse into itself.
  #
  # ufront 2.44: both halves live in the fetched package now, as
  # METOOLKIT_DECOMP_CENSUS. Forwarding rather than reimplementing keeps ONE
  # definition of what "instrumented" means; the flag has to be set before the
  # package is configured, which the top-level CMakeLists does.
  option(KD_CENSUS "Instrument the recovered Karma objects with a per-function call counter" OFF)
  if(KD_CENSUS)
    if(NOT METOOLKIT_DECOMP_CENSUS)
      message(FATAL_ERROR
        "KD_CENSUS=ON needs METOOLKIT_DECOMP_CENSUS=ON as well — the instrumentation is a "
        "property of the fetched library and has to be set before it is configured:\n"
        "  cmake --preset native -B build-native-census -DKD_CENSUS=ON "
        "-DMETOOLKIT_DECOMP_CENSUS=ON -DCMAKE_EXE_LINKER_FLAGS=-no-pie\n"
        "⚠ -no-pie is load-bearing and goes on the EXECUTABLE: kd_instr.c records RUNTIME "
        "addresses and nm prints link-time offsets, so in a PIE the census reads zero — "
        "indistinguishable from 'the physics never ran'.")
    endif()
    message(STATUS "Engine: KD_CENSUS -> recovered objects are instrumented (NOT a shipping build)")
  endif()

  target_link_libraries(${PROJECT_NAME} PRIVATE KarmaDecomp)
  message(STATUS "Engine: USE_KARMA_DECOMP -> metoolkit_decomp::all (fetched)")
endif()

