#=============================================================================
# Core - Engine foundation module
#
# Object system, memory management, file I/O, math, serialization,
# UnrealScript VM infrastructure.
#=============================================================================

project(Core CXX)

set(SRC_FILES
  "Src/Core.cpp"
  "Src/UExporter.cpp"
  "Src/UFactory.cpp"
  "Src/UnAnsi.cpp"
  "Src/UnArc.cpp"
  "Src/UnBits.cpp"
  "Src/UnCache.cpp"
  "Src/UnClass.cpp"
  "Src/UnCorSc.cpp"
  "Src/UnCoreNative.cpp"
  "Src/UnCoreNet.cpp"
  "Src/UnLinker.cpp"
  "Src/UnMath.cpp"
  "Src/UnMem.cpp"
  "Src/UnMisc.cpp"
  "Src/UnName.cpp"
  "Src/UnObj.cpp"
  "Src/UnProp.cpp"
  "Src/UnUnix.cpp"
  "Src/UnGUID.cpp"
)

# Work around GCC optimization issues with FFrame::Step() reference juggling
# (per UE1-fgsfdsfgs Core pattern)
if(NOT MSVC AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  set_source_files_properties("Src/UnCorSc.cpp" PROPERTIES COMPILE_FLAGS -Og)
  set_source_files_properties("Src/UnProp.cpp" PROPERTIES COMPILE_FLAGS -Og)
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=Core
)

# Core links zlib and needs ogg/vorbis include paths (per Core/Src/makefile)
target_link_libraries(${PROJECT_NAME} PRIVATE zlib ogg vorbis)
