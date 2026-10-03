cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" PROJECT_VERSION)
project(checksum VERSION ${PROJECT_VERSION})

option(CHECKSUM_TESTS "Configure test cases" ${PROJECT_IS_TOP_LEVEL})
option(CHECKSUM_BENCHMARKS "Configure benchmarks (Google Benchmark); on in the *-bench presets" OFF)
option(CHECKSUM_EXAMPLES "Configure examples" ${PROJECT_IS_TOP_LEVEL})
option(CHECKSUM_ACCELERATION "Use the CPU instructions (CRC, carry-less multiplication, vector) that the compiler flags enable" ON)

include(cmake/requirements.cmake)

add_subdirectory(src)

if (CHECKSUM_TESTS OR CHECKSUM_BENCHMARKS)
  include(CTest)
  add_subdirectory(tests)
endif ()

if (CHECKSUM_EXAMPLES)
  add_subdirectory(examples)
endif ()
