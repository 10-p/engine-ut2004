# OpenALMingw.cmake — the OpenAL Soft runtime DLL for the MinGW-w64 (Windows) build.
#
# UT2004's ALAudio is DYNAMIC_BIND: it asks the platform layer for "OpenAL32.dll" by name at run time and
# resolves every entry point from the handle. So this build needs the DLL and nothing else (no import
# library, no headers, no link-time dependency), and a missing DLL degrades to "no sound" instead of "will
# not start". A run-time LoadLibrary does not appear in the exe's import table, so the import table does
# not show that the DLL is needed.
#
# Nothing binary is vendored into git; this fetches OpenAL Soft's own official prebuilt Windows binaries.
# Sets OPENAL_MINGW_RUNTIME_DLL (+ _NAME) for the launcher's POST_BUILD copy.

set(OPENAL_MINGW_VERSION "1.25.2" CACHE STRING "OpenAL Soft release to fetch for the MinGW build")
set(OPENAL_MINGW_SHA256  "67a0c4b800bd860c93c04f38caf8cbe4875f9c84700ac430efc451f70e265434"
    CACHE STRING "SHA256 of openal-soft-<version>-bin.zip")

if(MINGW_TARGET_ARCH STREQUAL "x86_64")
  set(_OPENAL_MINGW_ARCHDIR "Win64")
else()
  set(_OPENAL_MINGW_ARCHDIR "Win32")
endif()

include(FetchContent)
FetchContent_Declare(OpenALMingw
  URL "https://github.com/kcat/openal-soft/releases/download/${OPENAL_MINGW_VERSION}/openal-soft-${OPENAL_MINGW_VERSION}-bin.zip"
  URL_HASH "SHA256=${OPENAL_MINGW_SHA256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
message(STATUS "OpenAL Soft (MinGW): fetching prebuilt ${OPENAL_MINGW_VERSION} for ${_OPENAL_MINGW_ARCHDIR}")
FetchContent_MakeAvailable(OpenALMingw)

set(_OPENAL_MINGW_ROOT "${openalmingw_SOURCE_DIR}")

# `soft_oal.dll` is the implementation; `router/` holds the pass-through OpenAL32.dll that dispatches to a
# system-installed driver. The implementation is staged, under the name ALAudio asks LoadLibrary for
# (AL_DLL in ALAudioSubsystem.h).
if(NOT EXISTS "${_OPENAL_MINGW_ROOT}/bin/${_OPENAL_MINGW_ARCHDIR}/soft_oal.dll")
  message(FATAL_ERROR "OpenAL Soft (MinGW): bin/${_OPENAL_MINGW_ARCHDIR}/soft_oal.dll is missing — the zip layout changed.")
endif()

set(OPENAL_MINGW_RUNTIME_DLL "${_OPENAL_MINGW_ROOT}/bin/${_OPENAL_MINGW_ARCHDIR}/soft_oal.dll"
    CACHE INTERNAL "OpenAL Soft implementation DLL to ship beside the executable")
set(OPENAL_MINGW_RUNTIME_DLL_NAME "OpenAL32.dll"
    CACHE INTERNAL "Name ALAudio's LoadLibrary asks for")
