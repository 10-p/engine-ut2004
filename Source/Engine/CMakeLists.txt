#=============================================================================
# Engine - core simulation module
#
# Actor system, game loop tick, physics (the Karma bridge), rendering interface, networking base,
# AI framework. The K*.cpp Karma bridge compiles under WITH_KARMA (both Karma arms below); a no-Karma
# build defines NO_KARMA and compiles their Karma contents out.
#=============================================================================

project(Engine CXX)

# Engine has ~115+ source files; GLOB is fine for this tree, where files are rarely added.
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

# From Engine/Src/makefile LIBS: Core, ogg, vorbis (the Karma libraries are linked below, per arm)
target_link_libraries(${PROJECT_NAME} PRIVATE Core zlib ogg vorbis)

# ---------------------------------------------------------------------------
# BUILD_KARMA_REF: MathEngine's own prebuilt archives (the A/B reference arm; see the option's note in the
# top-level CMakeLists). The compile side is the recovered arm's (WITH_KARMA=1 and the same public
# headers); only the link differs: a glob of MathEngine's shipped archives instead of
# `metoolkit_decomp::all`.
#
# --start-group is required: the archives have circular inter-dependencies (McdCommon <->
# McdPrimitives, Mst -> McdSpace/Mdt), so the linker has to re-scan until the symbol set closes.
if(BUILD_KARMA_REF)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KARMA=1)
  # METOOLKIT_DIR + METOOLKIT_LIB_SUBDIR are set at the top level from the FetchContent'd
  # metoolkit-karma-decomp (the Karma archives are not in this repository). Both are CACHE variables
  # there, so an override wins; point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at a local checkout
  # to build offline.
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
  set(METOOLKIT_LIB_DIR "${METOOLKIT_DIR}/${METOOLKIT_LIB_SUBDIR}")
  # An empty glob would link a WITH_KARMA engine against no Karma at all (every Karma symbol undefined at
  # the final exe link, far from the cause), hence the count check.
  file(GLOB METOOLKIT_LIBS "${METOOLKIT_LIB_DIR}/*.a")
  list(LENGTH METOOLKIT_LIBS METOOLKIT_LIB_COUNT)
  if(METOOLKIT_LIB_COUNT EQUAL 0)
    message(FATAL_ERROR "BUILD_KARMA_REF: no archives in ${METOOLKIT_LIB_DIR} (set METOOLKIT_DIR/METOOLKIT_LIB_SUBDIR).")
  endif()
  # Linux only: Windows is refused at configure time (top-level CMakeLists, BUILD_KARMA_REF's note).
  # The glibc-compat shim for the pre-2.3 archives (KarmaGlibcCompat.c) lives inside the group so its
  # __ctype_b/toupper/tolower definitions resolve during the group scan. The gcc-3.x archives otherwise
  # link against a modern gcc with no C++ ABI mismatch, only DT_TEXTREL warnings.
  add_library(KarmaGlibcCompat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Src/KarmaGlibcCompat.c)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    -Wl,--start-group ${METOOLKIT_LIBS} $<TARGET_FILE:KarmaGlibcCompat> -Wl,--end-group)
  message(STATUS "Engine: BUILD_KARMA_REF (LEGACY Karma) -> ${METOOLKIT_LIB_DIR} (${METOOLKIT_LIB_COUNT} archives)")
endif()

# ---------------------------------------------------------------------------
# USE_KARMA_DECOMP: the recovered Karma, compiled from source.
#
# WITH_KARMA=1 + metoolkit's public headers; it links `metoolkit_decomp::all` instead of the gcc-3.2 x86
# archives MathEngine shipped: 145 objects recovered from those archives, plus a hand-written
# convex-hull replacement for qhull. The sources are https://github.com/10-p/metoolkit-karma-decomp,
# fetched by the top-level CMakeLists, with the SDK, the toolchain that produces them and the gates that
# judge them. This repository holds the engine-side bridge only.
if(USE_KARMA_DECOMP)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KARMA=1)
  # METOOLKIT_DIR is set at the top level, from the FetchContent'd metoolkit-karma-decomp. It is a CACHE
  # PATH there, so an override on the command line wins.
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

  # The library is the fetched CMake target `metoolkit_decomp::all`, which owns the flags, the include
  # paths and the ME_FORCE_CDECL define. Its flags are required: decompiled C puns pointers through
  # integer slots, so -Wno-int-conversion / -Wno-incompatible-pointer-types are load-bearing, and
  # -fno-strict-aliasing keeps the compiler from miscompiling storage the recovery reinterprets as the
  # original machine code did (metoolkit_decomp/CMakeLists.txt). No --start-group: the package collects
  # every object into one archive, where the linker sees the whole symbol set at once.
  if(NOT TARGET metoolkit_decomp::all)
    message(FATAL_ERROR
      "USE_KARMA_DECOMP: metoolkit_decomp::all is not defined. The top-level "
      "CMakeLists FetchContent's https://github.com/10-p/metoolkit-karma-decomp; "
      "if you are offline, point FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at "
      "a local checkout of it.")
  endif()
  # UE_ENGINE_TU_FILES: a per-source measurement knob (the Engine-side twin of UE_KARMA_O2_FILES), a
# regex over this module's source basenames; matching sources get UE_ENGINE_TU_OPTIONS (a ;-list) appended
# after the module flags, e.g. one file at -O1 or with -fsanitize=undefined. Empty by default.
set(UE_ENGINE_TU_FILES "" CACHE STRING "regex over Engine source basenames to compile with UE_ENGINE_TU_OPTIONS (measurement aid)")
set(UE_ENGINE_TU_OPTIONS "" CACHE STRING "compile options (;-list) for the sources UE_ENGINE_TU_FILES matches")
if(NOT UE_ENGINE_TU_FILES STREQUAL "")
  file(GLOB _ut_tu_srcs "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cpp")
  set(_ut_tu_n 0)
  foreach(_ut_s IN LISTS _ut_tu_srcs)
    get_filename_component(_ut_b "${_ut_s}" NAME)
    if(_ut_b MATCHES "${UE_ENGINE_TU_FILES}")
      set_source_files_properties("${_ut_s}" PROPERTIES COMPILE_OPTIONS "${UE_ENGINE_TU_OPTIONS}")
      math(EXPR _ut_tu_n "${_ut_tu_n}+1")
    endif()
  endforeach()
  message(STATUS "Engine: ${_ut_tu_n} source(s) compiled with '${UE_ENGINE_TU_OPTIONS}' via UE_ENGINE_TU_FILES='${UE_ENGINE_TU_FILES}'")
endif()
add_library(KarmaDecomp INTERFACE)
  target_link_libraries(KarmaDecomp INTERFACE metoolkit_decomp::all)

  # KD_CENSUS: count every recovered function the real game enters. Off by default; it changes the code
  # generation of all 145 objects and is never a shipping build.
  #
  # "This object was substituted and the trajectory did not change" and "this object never executed"
  # produce the same output from every gate of the recovery's standalone tier (what its kd_instr.c is
  # for). Against a live match, a whole-library A/B that reads bit-identical proves the recovery exact
  # on the paths the map exercised and says nothing about the rest; this shows which those are.
  #
  #   cmake --preset native -B build-native-census -DKD_CENSUS=ON \
  #         -DCMAKE_EXE_LINKER_FLAGS=-no-pie
  #
  # -no-pie goes on the executable: kd_instr.c records the run-time address of each function, `nm`
  # prints link-time offsets, and in a PIE those differ by the load bias, so every address would resolve
  # to nothing ("0 of 2025 functions entered"). On this static library it does nothing. census_report.py
  # refuses a zero rather than printing one.
  #   KD_INSTR_OUT=/tmp/census.txt <binary> <map> ...
  #   python3 <metoolkit-karma-decomp>/decomp/tools/census_report.py /tmp/census.txt <binary>
  #
  # -finstrument-functions is applied only to the recovered objects, so the engine, the shipped library
  # and libc cost nothing; kd_instr.c is a separate TU compiled without the flag, or the counter would
  # recurse into itself. Both halves live in the fetched package as METOOLKIT_DECOMP_CENSUS; forwarding
  # keeps one definition of "instrumented". The flag is set before the package is configured (the
  # top-level CMakeLists does that).
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

