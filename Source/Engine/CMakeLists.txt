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

# Karma reference build (ufront 2.12 Part B — restores the pruned BUILD_KARMA_REF path). When ON, Engine
# compiles WITH_KARMA=1 (NO_KARMA left undefined at the top level) so the K*.cpp bridge activates its real
# Karma code paths, and links the prebuilt 32-bit metoolkit static libs (release variant). NOTE: these are
# gcc-3.2-era archives; the API is largely C-linkage, but a link failure here is the expected finding of
# "exercising the path" on modern gcc.
if(BUILD_KARMA_REF)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KARMA=1)
  # METOOLKIT_DIR + subdir are overridable so the proprietary Karma libs can live OUTSIDE the source
  # release (point at an external / fetched copy, e.g. an archival metoolkit-karma checkout) instead of
  # being bundled. Default: the in-tree copy + its newest Linux toolchain (gcc3.2).
  set(METOOLKIT_DIR "${CMAKE_SOURCE_DIR}/Thirdparty/metoolkit" CACHE PATH "metoolkit (Karma SDK) root: include/ + lib.{chk,rel}/")
  set(METOOLKIT_LIB_SUBDIR "lib.rel/linux_single_gcc3.2" CACHE STRING "metoolkit lib variant (rel/chk + toolchain)")
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
  # Link ALL metoolkit archives inside a --start-group: these libs have circular inter-dependencies
  # (e.g. McdCommon <-> McdPrimitives, Mst -> McdSpace/Mdt), so a plain ordered list leaves undefined
  # references. The group makes ld re-scan until the symbol set closes. The gcc-3.x archives link fine
  # against modern gcc (verified gcc3.2 + gcc3.1 -> gcc14: no C++ ABI mismatch; only DT_TEXTREL warnings).
  file(GLOB METOOLKIT_LIBS "${METOOLKIT_LIB_DIR}/*.a")
  list(LENGTH METOOLKIT_LIBS METOOLKIT_LIB_COUNT)
  if(METOOLKIT_LIB_COUNT EQUAL 0)
    message(FATAL_ERROR "BUILD_KARMA_REF: no .a libs in ${METOOLKIT_LIB_DIR} (set METOOLKIT_DIR/METOOLKIT_LIB_SUBDIR).")
  endif()
  # glibc-compat shim for pre-2.3 archives (see KarmaGlibcCompat.c). It lives INSIDE the group so its
  # __ctype_b/toupper/tolower definitions resolve the archives' references during the group scan.
  add_library(KarmaGlibcCompat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Src/KarmaGlibcCompat.c)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    -Wl,--start-group ${METOOLKIT_LIBS} $<TARGET_FILE:KarmaGlibcCompat> -Wl,--end-group)
  message(STATUS "Engine: BUILD_KARMA_REF -> ${METOOLKIT_LIB_DIR} (${METOOLKIT_LIB_COUNT} libs)")
endif()

