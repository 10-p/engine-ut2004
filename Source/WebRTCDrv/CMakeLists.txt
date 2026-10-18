#=============================================================================
# WebRTCDrv - WebRTC network driver for browser multiplayer
#
# Routes UE2 network packets through the host's WebRTC data channels. Web only.
#=============================================================================

project(WebRTCDrv CXX)

set(SRC_FILES
  "Src/WebRTCDrv.cpp"
  "Src/WebRTCNetDriver.cpp"
  "Src/WebRTCConnection.cpp"
  "Src/WebRTCDiscovery.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

# vendor/glue is PUBLIC: WebRTCDrv.h includes ue_abi.h, and SDLLaunch includes WebRTCDrv.h.
target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc ${CMAKE_SOURCE_DIR}/vendor/glue
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageWebRTCDrv
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
