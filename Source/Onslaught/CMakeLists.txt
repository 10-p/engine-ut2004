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

#-----------------------------------------------------------------------------
# Jolt Physics vehicle integration (replaces WITH_KARMA blocks in ONS*.cpp)
#-----------------------------------------------------------------------------
if(WITH_PHYSICS)
  # Add Jolt vehicle source files
  target_sources(${PROJECT_NAME} PRIVATE
    "Src/Jolt/JoltONSWheeledCraft.cpp"
    "Src/Jolt/JoltONSHoverCraft.cpp"
    "Src/Jolt/JoltONSChopperCraft.cpp"
    "Src/Jolt/JoltONSPlaneCraft.cpp"
    "Src/Jolt/JoltONSTreadCraft.cpp"
    "Src/Jolt/JoltONSVehicle.cpp"
  )

  # Define WITH_PHYSICS=1 (enables combined guards in Onslaught headers)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_PHYSICS=1)

  # Link Jolt Physics library
  target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)

  # Jolt requires C++17 — apply only to Jolt source files to avoid
  # noexcept conflict with UE2's operator new/delete in original ONS*.cpp
  set_source_files_properties(
    "Src/Jolt/JoltONSWheeledCraft.cpp"
    "Src/Jolt/JoltONSHoverCraft.cpp"
    "Src/Jolt/JoltONSChopperCraft.cpp"
    "Src/Jolt/JoltONSPlaneCraft.cpp"
    "Src/Jolt/JoltONSTreadCraft.cpp"
    "Src/Jolt/JoltONSVehicle.cpp"
    PROPERTIES COMPILE_FLAGS "-std=c++17"
  )

  # Add Engine/Src/Jolt to include path for JoltAdapter.h and JoltGlobals.h
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Source/Engine/Src/Jolt
  )

  message(STATUS "Onslaught: WITH_PHYSICS enabled, Jolt vehicle sources included")
endif()
