#=============================================================================
# Onslaught - Onslaught game mode
#
# ONS game mode, vehicle variants, power node system.
#=============================================================================

project(Onslaught CXX)

set(SRC_FILES
  "Src/ONSChopperCraft.cpp"
  "Src/ONSHoverCraft.cpp"
  "Src/ONSPlaneCraft.cpp"
  "Src/ONSTreadCraft.cpp"
  "Src/ONSVehicle.cpp"
  "Src/ONSWeapon.cpp"
  "Src/ONSWheeledCraft.cpp"
  "Src/Onslaught.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageOnslaught
  ThisPackage=Onslaught
)

# From Onslaught/Src/makefile: includes XGame and IpDrv headers
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine XGame IpDrv)
