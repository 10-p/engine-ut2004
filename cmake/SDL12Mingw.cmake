# SDL12Mingw.cmake — SDL 1.2 for the MinGW-w64 (Windows) build of the LEGACY graphics backend.
#
# ufront step 2.33. The legacy stack is SDL 1.2 (SDLDrv) + desktop OpenGL (OpenGLDrv), and it is the right
# stack for Windows: GL4ES translates desktop GL to GLES2, which Linux drivers ship and Windows drivers
# generally do not — on Windows the real `opengl32` is simply there, so the translation layer buys nothing
# and costs a dependency that does not exist. (Owner's call; Linux keeps BOTH stacks.)
#
# In tree, SDL 1.2 is a vendored 2006 Linux `.so`. There is no Windows equivalent to vendor, so fetch
# SDL's own official MinGW development binaries — the same FetchContent-a-pinned-archive pattern the other
# engines use for SDL2.
#
# ⚠ This is an IMPORT library (`libSDL.dll.a`), not a static one: `SDL.dll` must ship beside the .exe.
# Unlike the UE1 engines, the ut2004 Windows build is therefore not a single self-contained file.
#
# Sets SDL12_INCLUDE_DIR and SDL12_LIBRARY.

set(SDL12_MINGW_VERSION "1.2.15" CACHE STRING "SDL 1.2 release to fetch for the MinGW build")
set(SDL12_MINGW_SHA256  "d51eedfe7e07893d6c93a2d761c6ccc91d04b5f68a2ecabdbef83b7a1fef9cde"
    CACHE STRING "SHA256 of SDL-devel-<version>-mingw32.tar.gz")

include(FetchContent)
FetchContent_Declare(SDL12Mingw
  URL "https://www.libsdl.org/release/SDL-devel-${SDL12_MINGW_VERSION}-mingw32.tar.gz"
  URL_HASH "SHA256=${SDL12_MINGW_SHA256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
message(STATUS "SDL 1.2 (MinGW): fetching prebuilt ${SDL12_MINGW_VERSION}")
FetchContent_MakeAvailable(SDL12Mingw)

# ⚠ The archive carries AppleDouble (`._name`) entries beside its root directory, so FetchContent does not
# see a single top-level dir and does not strip one — the tree lands one level deeper than usual. Resolve
# it by finding the library rather than assuming either shape.
file(GLOB_RECURSE _SDL12_IMPLIB "${sdl12mingw_SOURCE_DIR}/*/libSDL.dll.a" "${sdl12mingw_SOURCE_DIR}/libSDL.dll.a")
list(GET _SDL12_IMPLIB 0 _SDL12_IMPLIB)
if(NOT _SDL12_IMPLIB OR NOT EXISTS "${_SDL12_IMPLIB}")
  message(FATAL_ERROR "SDL 1.2 (MinGW): libSDL.dll.a not found under ${sdl12mingw_SOURCE_DIR} — the tarball layout changed.")
endif()
get_filename_component(_SDL12_LIBDIR "${_SDL12_IMPLIB}" DIRECTORY)
get_filename_component(_SDL12_ROOT   "${_SDL12_LIBDIR}" DIRECTORY)

# The engine includes <SDL.h> directly, and this tarball nests the headers under include/SDL/.
set(SDL12_INCLUDE_DIR "${_SDL12_ROOT}/include/SDL")
set(SDL12_LIBRARY
  -lmingw32
  "${_SDL12_LIBDIR}/libSDLmain.a"
  "${_SDL12_IMPLIB}"
)
