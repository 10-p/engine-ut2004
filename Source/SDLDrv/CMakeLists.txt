#=============================================================================
# SDLDrv - SDL viewport/input driver
#
# SDL 1.2 based windowing, input, and viewport management for Linux/Mac.
#=============================================================================

project(SDLDrv CXX)

set(SRC_FILES
  "Src/SDLClient.cpp"
  "Src/SDLDrv.cpp"
  "Src/SDLViewport.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${SDL12_INCLUDE_DIR}
)

# From SDLDrv/Src/makefile: -DGPackage=SDLDrv (no GPackage prefix for this module)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=SDLDrv
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
