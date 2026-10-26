#=============================================================================
# ALAudio - OpenAL audio subsystem
#
# OpenAL-based audio with EAX support and Ogg/Vorbis streaming. Voice capture (Speex) is compiled out
# (WITH_SPEEX=0).
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
  # Compile against openal-soft's headers (its include dir precedes the Emscripten sysroot's <AL/al.h>).
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL)
endif()
if(ANDROID)
  # SDL's application events pause the output device while the app is in the background.
  target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
endif()
