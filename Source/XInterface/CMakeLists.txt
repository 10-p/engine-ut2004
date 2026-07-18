#=============================================================================
# XInterface - In-game HUD and menus
#
# HUD base, GUI framework, server browser UI, menu components.
#=============================================================================

project(XInterface CXX)

set(SRC_FILES
  "Src/AHudBase.cpp"
  "Src/GUILists.cpp"
  "Src/UnGUI.cpp"
  "Src/UnGUICore.cpp"
  "Src/XInterface.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Source/Window/Inc
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageXInterface
  ThisPackage=XInterface
)

# From XInterface/Src/makefile LIBS: Core, Engine, IpDrv, XGame
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine IpDrv XGame)
