#=============================================================================
# SDL2Drv - SDL2 viewport/input driver
#
# SDL2 based windowing, input, and viewport management.
# Replaces the legacy SDL 1.2 SDLDrv module.
#=============================================================================

project(SDL2Drv CXX)

set(SRC_FILES
  "Src/SDL2Client.cpp"
  "Src/SDL2Drv.cpp"
  "Src/SDL2Viewport.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=SDL2Drv
)

if(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine SDL2::SDL2-static)
endif()
