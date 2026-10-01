cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" PROJECT_VERSION)
project(checksum VERSION ${PROJECT_VERSION})

option(CHECKSUM_TESTS "Configure test cases" ${PROJECT_IS_TOP_LEVEL})

include(CTest)
include(cmake/requirements.cmake)

add_subdirectory(src)

if (CHECKSUM_TESTS)
  add_subdirectory(tests)
endif ()
