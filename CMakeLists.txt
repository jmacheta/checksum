cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" PROJECT_VERSION)
project(checksum VERSION ${PROJECT_VERSION})

option(CHECKSUM_TESTS "Configure test cases and benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(CHECKSUM_EXAMPLES "Configure examples" ${PROJECT_IS_TOP_LEVEL})
option(CHECKSUM_ACCELERATION "Use the CPU instructions (CRC, carry-less multiplication, vector) that the compiler flags enable" ON)

include(cmake/requirements.cmake)

add_subdirectory(src)

if (CHECKSUM_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif ()

if (CHECKSUM_EXAMPLES)
  add_subdirectory(examples)
endif ()
