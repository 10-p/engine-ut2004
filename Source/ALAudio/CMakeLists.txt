#=============================================================================
# ALAudio - OpenAL audio subsystem
#
# OpenAL-based audio with EAX support, Ogg/Vorbis streaming,
# Speex VoIP encode/decode.
#=============================================================================

project(ALAudio CXX)

set(SRC_FILES
  "Src/ALAudio.cpp"
  "Src/ALAudioStream.cpp"
  "Src/ALAudioSubsystem.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Thirdparty/OpenAL/Inc
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageALAudio
  ThisPackage=ALAudio
  AILCALL=
  WITH_SPEEX=0
)

# From ALAudio/Src/makefile LIBS: Core, Engine, ogg, vorbis (Speex removed with WITH_SPEEX=0)
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine ogg vorbis)
if(EMSCRIPTEN)
  # ufront 2.61: compile against openal-soft's headers (its include dir precedes the Emscripten sysroot's
  # <AL/al.h>, which belonged to the -lopenal port this replaces).
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL)
endif()
