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
# BUILD_KARMA_REF — the LEGACY Karma: MathEngine's own prebuilt archives.
#
# The A/B reference arm (see the option's note in the top-level CMakeLists). Compile side is
# identical to the recovered arm — WITH_KARMA=1 and the same public headers — and only the LINK
# differs: a glob of MathEngine's shipped archives instead of `metoolkit_decomp::all`.
#
# ⚠ --start-group IS NOT OPTIONAL. These archives have circular inter-dependencies (McdCommon <->
# McdPrimitives, Mst -> McdSpace/Mdt), so a plain ordered list leaves undefined references however
# it is sorted; the group makes the linker re-scan until the symbol set closes.
if(BUILD_KARMA_REF)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KARMA=1)
  # METOOLKIT_DIR + METOOLKIT_LIB_SUBDIR are set at the top level from the FetchContent'd
  # metoolkit-karma-decomp (ufront 2.44) — the proprietary Karma archives are not bundled in this
  # repository. Both are CACHE variables there, so an override still wins; point
  # FETCHCONTENT_SOURCE_DIR_METOOLKITKARMADECOMP at a local checkout to build offline.
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
  # ⚠ THE ARCHIVE EXTENSION IS THE PLATFORM'S, not a detail: MathEngine shipped ELF `.a` for Linux
  # and MS COFF `.lib` for Windows. A glob for the wrong one matches nothing, and an empty glob
  # would otherwise link a WITH_KARMA engine against no Karma at all — every Karma symbol
  # undefined at the FINAL exe link, hundreds of lines away from the cause. Hence the count check.
  if(WIN32)
    file(GLOB METOOLKIT_LIBS "${METOOLKIT_LIB_DIR}/*.lib")
  else()
    file(GLOB METOOLKIT_LIBS "${METOOLKIT_LIB_DIR}/*.a")
  endif()
  list(LENGTH METOOLKIT_LIBS METOOLKIT_LIB_COUNT)
  if(METOOLKIT_LIB_COUNT EQUAL 0)
    message(FATAL_ERROR "BUILD_KARMA_REF: no archives in ${METOOLKIT_LIB_DIR} (set METOOLKIT_DIR/METOOLKIT_LIB_SUBDIR).")
  endif()
  if(WIN32)
    # KarmaMsvcCompat is the Windows counterpart of KarmaGlibcCompat below, and it is needed for
    # the same reason: these are the vendor's ORIGINAL binaries and they call their original
    # compiler's support library. MinGW links msvcrt itself — which is why `win32_single_msvcrt`
    # is the variant used here and not `win32_single_libcmt`, whose archives want MSVC's static
    # CRT — but msvcrt.dll does not export MSVC's COMPILER helpers (_chkstk, _alldiv, ...), and
    # libgcc spells every one of them differently. See the file for the six and their ABIs.
    # It goes INSIDE the group so the archives' references resolve during the scan.
    add_library(KarmaMsvcCompat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Src/KarmaMsvcCompat.c)
    # ★ --exclude-libs FOR THE VENDOR ARCHIVES, and the exe does not link without it.
    #
    # SDLLaunch links with --export-all-symbols (the game's .u/.dll modules resolve against the
    # exe, so it needs a real export table and an import library). That sweep walks every archive
    # it pulled, and on MathEngine's MSVC-built C++ objects it dies with a screenful of
    #   "cannot export ?solveLCP@keaLCPSolver@@QAEHPAV...: symbol not found"
    # — MSVC-mangled COMDAT members that appear in the archive's symbol index while their section
    # is discarded, so ld puts them on the export list and then cannot find them. Nothing outside
    # these archives should be exported from a Karma implementation anyway, so excluding them from
    # the auto-export sweep is both the fix and the correct description of intent.
    #
    # Built from METOOLKIT_LIBS rather than written out, so a change to the SDK's contents cannot
    # leave a stale name behind: --exclude-libs silently ignores a name that matches no archive,
    # which is exactly how this would rot without being noticed.
    set(METOOLKIT_EXCLUDE "")
    foreach(_mk_lib ${METOOLKIT_LIBS})
      get_filename_component(_mk_name "${_mk_lib}" NAME)
      if(METOOLKIT_EXCLUDE STREQUAL "")
        set(METOOLKIT_EXCLUDE "${_mk_name}")
      else()
        set(METOOLKIT_EXCLUDE "${METOOLKIT_EXCLUDE}:${_mk_name}")
      endif()
    endforeach()
    target_link_libraries(${PROJECT_NAME} PRIVATE
      -Wl,--exclude-libs,${METOOLKIT_EXCLUDE}
      -Wl,--start-group ${METOOLKIT_LIBS} $<TARGET_FILE:KarmaMsvcCompat> -Wl,--end-group)
  else()
    # The glibc-compat shim for the pre-2.3 archives (see KarmaGlibcCompat.c) lives INSIDE the
    # group so its __ctype_b/toupper/tolower definitions resolve during the group scan. The
    # gcc-3.x archives otherwise link fine against a modern gcc — verified gcc3.2 + gcc3.1 ->
    # gcc14: no C++ ABI mismatch, only DT_TEXTREL warnings.
    add_library(KarmaGlibcCompat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Src/KarmaGlibcCompat.c)
    target_link_libraries(${PROJECT_NAME} PRIVATE
      -Wl,--start-group ${METOOLKIT_LIBS} $<TARGET_FILE:KarmaGlibcCompat> -Wl,--end-group)
  endif()
  message(STATUS "Engine: BUILD_KARMA_REF (LEGACY Karma) -> ${METOOLKIT_LIB_DIR} (${METOOLKIT_LIB_COUNT} archives)")
endif()

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
  # ufront 2.58 — a per-source measurement knob, the Engine-side twin of UT_KARMA_O2_FILES: UT_ENGINE_TU_FILES is
# a regex over this module's source basenames; matching sources get UT_ENGINE_TU_OPTIONS (a ;-list) appended
# after the module flags. It is how the -SOFTWARERENDERER mid-match crash was localised to UnSkeletalMesh.cpp
# (that one file at -O1, at -O2 -fsanitize=undefined, and finally with two null-reference sites guarded, each
# ran ONS-Torlan for 300 s where the plain -O2 build crashed 5 times in 8). See BUILD.md "ufront 2.58".
set(UT_ENGINE_TU_FILES "" CACHE STRING "regex over Engine source basenames to compile with UT_ENGINE_TU_OPTIONS (measurement aid)")
set(UT_ENGINE_TU_OPTIONS "" CACHE STRING "compile options (;-list) for the sources UT_ENGINE_TU_FILES matches")
if(NOT UT_ENGINE_TU_FILES STREQUAL "")
  file(GLOB _ut_tu_srcs "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cpp")
  set(_ut_tu_n 0)
  foreach(_ut_s IN LISTS _ut_tu_srcs)
    get_filename_component(_ut_b "${_ut_s}" NAME)
    if(_ut_b MATCHES "${UT_ENGINE_TU_FILES}")
      set_source_files_properties("${_ut_s}" PROPERTIES COMPILE_OPTIONS "${UT_ENGINE_TU_OPTIONS}")
      math(EXPR _ut_tu_n "${_ut_tu_n}+1")
    endif()
  endforeach()
  message(STATUS "Engine: ${_ut_tu_n} source(s) compiled with '${UT_ENGINE_TU_OPTIONS}' via UT_ENGINE_TU_FILES='${UT_ENGINE_TU_FILES}'")
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

