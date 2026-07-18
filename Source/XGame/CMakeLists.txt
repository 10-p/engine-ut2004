#=============================================================================
# XGame - UT2004 core game logic
#
# Bot AI, match types (DM, CTF, DOM, BR), game utilities.
#=============================================================================

project(XGame CXX)

set(SRC_FILES
  "Src/XGame.cpp"
  "Src/XUtil.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageXGame
  ThisPackage=XGame
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
