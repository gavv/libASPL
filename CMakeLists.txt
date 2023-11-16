cmake_minimum_required(VERSION 3.0.0)

project(aspl CXX)

option(BUILD_DOCUMENTATION "Build Doxygen documentation" OFF)

execute_process(
  OUTPUT_VARIABLE GIT_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND sh -c "cd \"${PROJECT_SOURCE_DIR}\" && git describe --tags --abbrev=0"
  )
string(REGEX MATCH "v?([0-9.]+)" RESULT ${GIT_TAG})
set(GIT_TAG "${CMAKE_MATCH_1}")

message(STATUS "Detected version from git: ${GIT_TAG}")

set(PACKAGE_NAME libASPL)
set(PACKAGE_VERSION ${GIT_TAG})

set(LIB_TARGET libASPL)
set(LIB_NAME ASPL)
set(TEST_NAME aspl-test)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(COMPILER_FLAGS "")
set(LINKER_FLAGS "")

list(APPEND COMPILER_FLAGS
  "-Wall"
  "-Wextra"
  "-Wno-invalid-offsetof"
  "-Wno-unused-parameter"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND COMPILER_FLAGS
    "-ggdb"
    "-funwind-tables"
    "-fno-omit-frame-pointer"
    )
endif()

option(ENABLE_SANITIZERS "enable clang sanitizer" OFF)

if(ENABLE_SANITIZERS)
  list(APPEND COMPILER_FLAGS
    "-fsanitize=address"
    )

  list(APPEND LINKER_FLAGS
    "-fsanitize=address"
    )
endif()

string(REPLACE ";" " " COMPILER_FLAGS "${COMPILER_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS}")

string(REPLACE ";" " " LINKER_FLAGS "${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

set(OBJECT_LIST
  "Device"
  "MuteControl"
  "Object"
  "Plugin"
  "Stream"
  "VolumeControl"
)

add_custom_target("gen")

foreach(CLASS IN LISTS OBJECT_LIST)
  add_custom_command(
    TARGET "gen"
    COMMAND cd "${PROJECT_SOURCE_DIR}" &&
      "${PROJECT_SOURCE_DIR}/scripts/generate-accessors.py"
        -i "src/${CLASS}.json"
        -o "src/${CLASS}.g.cpp"
    COMMENT
      "Generating src/${CLASS}.g.cpp"
    )

  list(APPEND SOURCE_LIST
    "${PROJECT_SOURCE_DIR}/src/${CLASS}.cpp"
    "${PROJECT_SOURCE_DIR}/src/${CLASS}.g.cpp"
    )
endforeach()

add_custom_command(
  TARGET "gen"
  COMMAND cd "${PROJECT_SOURCE_DIR}" &&
    "${PROJECT_SOURCE_DIR}/scripts/generate-bridge.py"
    -i "src/Bridge.json"
    -o "src/Bridge.g.cpp"
  COMMENT
    "Generating src/Bridge.g.cpp"
  )

list(APPEND SOURCE_LIST
  "${PROJECT_SOURCE_DIR}/src/Bridge.g.cpp"
  )

add_custom_command(
  TARGET "gen"
  COMMAND cd "${PROJECT_SOURCE_DIR}" &&
    "${PROJECT_SOURCE_DIR}/scripts/generate-strings.py"
    -c "${CMAKE_CXX_COMPILER}"
    -s "${CMAKE_OSX_SYSROOT}"
    -o "src/Strings.g.cpp"
  COMMENT
    "Generating src/Strings.g.cpp"
  )

list(APPEND SOURCE_LIST
  "${PROJECT_SOURCE_DIR}/src/Strings.g.cpp"
  )

list(APPEND SOURCE_LIST
  "src/Client.cpp"
  "src/Convert.cpp"
  "src/Dispatcher.cpp"
  "src/Driver.cpp"
  "src/Storage.cpp"
  "src/Strings.cpp"
  "src/Tracer.cpp"
  "src/Uid.cpp"
  "src/VolumeCurve.cpp"
  )

add_library(${LIB_TARGET} STATIC
  ${SOURCE_LIST}
  )

target_include_directories(${LIB_TARGET}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )

find_library(LIB_CoreFoundation CoreFoundation REQUIRED)
target_link_libraries(${LIB_TARGET}
  PUBLIC ${LIB_CoreFoundation}
  )

set_target_properties(${LIB_TARGET} PROPERTIES
  OUTPUT_NAME ${LIB_NAME}
  )

set_property(TARGET ${LIB_TARGET}
  PROPERTY VERSION ${PACKAGE_VERSION}
  )

set_property(TARGET ${LIB_TARGET}
  APPEND PROPERTY COMPATIBLE_INTERFACE_STRING ${PACKAGE_VERSION}
  )

install(TARGETS ${LIB_TARGET}
  EXPORT ${PACKAGE_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

install(DIRECTORY include/aspl
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include
  )

install(EXPORT ${PACKAGE_NAME}Targets
  FILE ${PACKAGE_NAME}Targets.cmake
  NAMESPACE aspl::
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/${PACKAGE_NAME}
  )

include(CMakePackageConfigHelpers)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
  INSTALL_DESTINATION lib/cmake/${PACKAGE_NAME}
  )

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}ConfigVersion.cmake
  VERSION ${PACKAGE_VERSION}
  COMPATIBILITY ExactVersion
  )

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/${PACKAGE_NAME}
  )

if(BUILD_TESTING)
  include(ExternalProject)
  ExternalProject_Add(googletest
    GIT_REPOSITORY      https://github.com/google/googletest.git
    GIT_TAG             release-1.12.1
    GIT_SHALLOW         ON
    UPDATE_DISCONNECTED 1
    SOURCE_DIR          ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
    BINARY_DIR          ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
    INSTALL_COMMAND     ""
    TEST_COMMAND        ""
    LOG_DOWNLOAD        ON
    LOG_CONFIGURE       ON
    LOG_BUILD           ON
    )

  add_dependencies(googletest
    ${LIB_TARGET}
    )

  add_executable(${TEST_NAME}
    "test/Main.cpp"
    "test/TestClients.cpp"
    "test/TestConstruction.cpp"
    "test/TestDoubleBuffer.cpp"
    "test/TestOperations.cpp"
    "test/TestProperties.cpp"
    "test/TestRegistration.cpp"
    "test/TestStorage.cpp"
    )

  add_dependencies(${TEST_NAME}
    ${LIB_TARGET}
    googletest
    )

  target_include_directories(${TEST_NAME} SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/googletest-src/googletest/include
    )

  target_link_libraries(${TEST_NAME}
    ${LIB_TARGET}
    ${CMAKE_CURRENT_BINARY_DIR}/googletest-build/lib/libgtest.a
    )

  enable_testing()
  add_test(NAME ${TEST_NAME}
    COMMAND ${TEST_NAME}
    )
endif(BUILD_TESTING)

if(BUILD_DOCUMENTATION)
  find_package(Doxygen)

  if(DOXYGEN_FOUND STREQUAL YES)
    add_custom_target(doxygen ALL
      DEPENDS ${LIB_TARGET}
      COMMENT "Generating HTML documentation"
      COMMAND cd "${PROJECT_SOURCE_DIR}" && doxygen
      )
  endif()
endif(BUILD_DOCUMENTATION)

add_custom_command(
  COMMENT "Copying compile_commands.json to project root"
  DEPENDS ${LIB_TARGET}
  OUTPUT ${PROJECT_SOURCE_DIR}/compile_commands.json
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${PROJECT_SOURCE_DIR}/compile_commands.json
  )
add_custom_target(compile_commands ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/compile_commands.json
)
