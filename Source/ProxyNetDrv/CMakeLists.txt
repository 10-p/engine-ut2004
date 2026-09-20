#=============================================================================
# ProxyNetDrv - "Proxy networking": join an ordinary NATIVE game server from the browser
#
# A client-only UNetDriver modelled on IpDrv's TcpNetDriver, with the BSD socket calls replaced by the
# OPTIONAL `proxynet` ABI module (vendor/glue/ue_proxynet_abi.h -> vendor/glue/proxynet-shim.js -> the
# host-injected Module.UE.proxynet, which speaks to a WSS<->UDP game proxy).
#
# EXPERIMENTAL and DETACHED BY DECISION (ufrontend docs/12 ADR-16): it shares nothing with WebRTCDrv and
# touches none of its files. Configure with -DUT_WITH_PROXYNET=OFF and this package, its shim and its
# registration all disappear; P2P networking is exactly what it was.
# Emscripten-only: a native build has real sockets and uses IpDrv.
#=============================================================================

project(ProxyNetDrv CXX)

set(SRC_FILES
  "Src/ProxyNetDrv.cpp"
  "Src/ProxyNetDriver.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glue
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageProxyNetDrv
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine)
