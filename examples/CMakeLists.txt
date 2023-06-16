set(TOPLEVEL_COMMENT "THIS FILE IS GENERATED FROM TEMPLATE. DO NOT EDIT!")

#
# NetcatDevice
#

set(DRIVER_NAME "NetcatDevice")
set(DRIVER_VERSION "1.0.0")
set(DRIVER_COPYRIGHT "Copyright (c) libASPL authors")
set(DRIVER_IDENTIFIER "netcatdevice.examples.libaspl")
set(DRIVER_UID "95963308-3BB7-48FC-A58D-5EC8E7578083")
set(DRIVER_ENTRYPOINT "ExampleEntryPoint")

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/_template/CMakeLists.txt.in
  ${CMAKE_CURRENT_LIST_DIR}/NetcatDevice/CMakeLists.txt
  @ONLY
)

file(COPY
  ${CMAKE_CURRENT_LIST_DIR}/_template/Info.plist.in
  DESTINATION ${CMAKE_CURRENT_LIST_DIR}/NetcatDevice)

add_subdirectory(NetcatDevice)

#
# SinewaveDevice
#

set(DRIVER_NAME "SinewaveDevice")
set(DRIVER_VERSION "1.0.0")
set(DRIVER_COPYRIGHT "Copyright (c) libASPL authors")
set(DRIVER_IDENTIFIER "sinewavedevice.examples.libaspl")
set(DRIVER_UID "30F8E35D-FB97-4A9C-A469-9038A751B4B6")
set(DRIVER_ENTRYPOINT "ExampleEntryPoint")

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/_template/CMakeLists.txt.in
  ${CMAKE_CURRENT_LIST_DIR}/SinewaveDevice/CMakeLists.txt
  @ONLY
)

file(COPY
  ${CMAKE_CURRENT_LIST_DIR}/_template/Info.plist.in
  DESTINATION ${CMAKE_CURRENT_LIST_DIR}/SinewaveDevice)

add_subdirectory(SinewaveDevice)
