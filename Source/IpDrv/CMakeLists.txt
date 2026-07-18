#=============================================================================
# IpDrv - TCP/IP networking driver
#
# TCP/UDP link classes, master server uplink, HTTP download support.
#=============================================================================

project(IpDrv CXX)

set(SRC_FILES
  "Src/HTTPDownload.cpp"
  "Src/InternetLink.cpp"
  "Src/IpDrv.cpp"
  "Src/MasterServerClient.cpp"
  "Src/MasterServerUplink.cpp"
  "Src/TcpLink.cpp"
  "Src/TcpNetDriver.cpp"
  "Src/UCompressCommandlet.cpp"
  "Src/UdpLink.cpp"
  "Src/UnIpDrvNative.cpp"
  "Src/UnSocket.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageIpDrv
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
