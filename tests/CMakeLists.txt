add_library(checksum_test_data INTERFACE)
target_sources(
  checksum_test_data
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            include
            FILES
            include/test_data.hpp
            include/isa_cpu.hpp
)

# Bare metal has no Google Test: one program checks the instruction-set layer there.
if (CHECKSUM_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
  add_subdirectory(bare_metal)
elseif (CHECKSUM_TESTS)
  add_subdirectory(unit_tests)
  add_subdirectory(compile_fail)
endif ()

if (CHECKSUM_BENCHMARKS)
  add_subdirectory(benchmarks)
endif ()
