#=============================================================================
# UWeb - Built-in HTTP web admin server
#
# HTTP server for remote game administration.
#=============================================================================

project(UWeb CXX)

set(SRC_FILES
  "Src/UWeb.cpp"
  "Src/UWebNative.cpp"
  "Src/WebServer.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  GPackage=GPackageUWeb
)

# From UWeb/Src/makefile: includes IpDrv headers, links Core and Engine
target_link_libraries(${PROJECT_NAME} PRIVATE Core Engine IpDrv)
