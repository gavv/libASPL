# THIS FILE IS GENERATED FROM TEMPLATE. DO NOT EDIT!
cmake_minimum_required(VERSION 3.0.0)

project(SinewaveDevice CXX)

# set compiler options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# set to non-empty string to enable code signing
set(CODESIGN_ID "" CACHE STRING "Codesign ID")

set(DRIVER_NAME "SinewaveDevice")
set(DRIVER_VERSION "1.0.0")
set(DRIVER_COPYRIGHT "Copyright (c) libASPL authors")
set(DRIVER_IDENTIFIER "sinewavedevice.examples.libaspl")
set(DRIVER_UID "30F8E35D-FB97-4A9C-A469-9038A751B4B6")
set(DRIVER_ENTRYPOINT "ExampleEntryPoint")

# report configuration to console
message(STATUS "Driver name - ${DRIVER_NAME}")
message(STATUS "Driver version - ${DRIVER_VERSION}")
message(STATUS "Driver copyright - ${DRIVER_COPYRIGHT}")
message(STATUS "Driver identifier - ${DRIVER_IDENTIFIER}")
message(STATUS "Driver codesign ID - ${CODESIGN_ID}")

# path to libASPL source directory
get_filename_component(LIBASPL_SOURCE_DIR
  ${CMAKE_CURRENT_LIST_DIR}/../..
  ABSOLUTE)

# add rules for building libASPL and installing it into a prefix
set(LIBASPL_TARGET SinewaveDevice_libASPL)
include(ExternalProject)
ExternalProject_Add(${LIBASPL_TARGET}
  SOURCE_DIR ${LIBASPL_SOURCE_DIR}
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libASPL-build
  INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libASPL-prefix
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  LOG_DOWNLOAD YES
  LOG_CONFIGURE YES
  LOG_BUILD YES
  LOG_INSTALL YES
  )

# we use MODULE to create .so library instead of .dylib library; .so files are
# also known as Mach-O loadable bundles (not to be confused with macOS bundle
# directory, which we create below)
# see:
#  - https://cmake.org/cmake/help/latest/command/add_library.html
#  - https://stackoverflow.com/a/2339910/3169754
add_library(${DRIVER_NAME} MODULE
  "Driver.cpp"
  )

# add libASPL dependency
add_dependencies(${DRIVER_NAME} ${LIBASPL_TARGET})
target_include_directories(${DRIVER_NAME}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libASPL-prefix/include
  )
target_link_libraries(${DRIVER_NAME}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libASPL-prefix/lib/libASPL.a
  )

# add CoreFoundation dependency
find_library(LIB_CoreFoundation CoreFoundation REQUIRED)
target_link_libraries(${DRIVER_NAME}
  PRIVATE ${LIB_CoreFoundation}
  )

# here we adjust properties of our shared library and ask CMake to pack it into
# macOS bundle directory
#
# we set OUTPUT_NAME to be "${DRIVER_NAME}"
# we set PREFIX to "" to remove "lib" prefix
# we set SUFFIX to "" to remove ".so" suffix
# resulting name of the shared library will be just "${DRIVER_NAME}"
#
# we set MACOSX_BUNDLE to "ON" and BUNDLE to "true" to instruct CMake
# to pack our shared library into macOS bundle directory (Finder may display that
# directory as a single opaque item)
#
# we set MACOSX_BUNDLE_BUNDLE_NAME to "${DRIVER_NAME}" and
# BUNDLE_EXTENSION to "driver", so that the resulting directory name
# will be "${DRIVER_NAME}.driver"
#
# we set MACOSX_BUNDLE_INFO_PLIST to the path of Info.plist.in template, which will be
# used by CMake to generate resulting Info.plist; CMake will substitute variables in
# template with CMake vars like MACOSX_BUNDLE_*
#
# the bundle directory will contain our shared library, which is a plugin for audio
# server, and "Info.plist" file, which is meta-information for the plugin, and
# optionally code signature
#
# $ tree build/Example/${DRIVER_NAME}.driver
# `-- Contents
#     |-- Info.plist
#     |-- MacOS
#     |   `-- ${DRIVER_NAME}
#     `-- _CodeSignature
#         `-- CodeResources
#
# the user can install the bundle directory into /Library/Audio/Plug-Ins/HAL; on start,
# audio server will find it, read Info.plist, load shared library into its address
# space, and invoke entry point function (see above) to construct our plugin; it will
# then use the plugin to create virtual audio device
set_target_properties(${DRIVER_NAME} PROPERTIES
  OUTPUT_NAME "${DRIVER_NAME}"
  BUNDLE true
  BUNDLE_EXTENSION "driver"
  PREFIX ""
  SUFFIX ""
  MACOSX_BUNDLE ON
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  MACOSX_BUNDLE_BUNDLE_NAME "${DRIVER_NAME}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${DRIVER_VERSION}"
  MACOSX_BUNDLE_COPYRIGHT "${DRIVER_COPYRIGHT}"
  MACOSX_BUNDLE_GUI_IDENTIFIER "${DRIVER_IDENTIFIER}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${DRIVER_VERSION}"
  )

# sign the bundle directory
# this will add _CodeSignature subdirectory to the bundle
if(NOT CODESIGN_ID STREQUAL "")
  add_custom_command(
    TARGET ${DRIVER_NAME}
    POST_BUILD
    COMMENT
      "Signing ${DRIVER_NAME}.driver with ID ${CODESIGN_ID}"
    VERBATIM
    COMMAND
      codesign --force -s "${CODESIGN_ID}"
        "${CMAKE_CURRENT_BINARY_DIR}/${DRIVER_NAME}.driver"
    )
endif()
