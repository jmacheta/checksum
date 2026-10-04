add_library(checksum_test_data INTERFACE)
target_sources(checksum_test_data INTERFACE FILE_SET HEADERS BASE_DIRS include FILES include/test_data.hpp)

if (CHECKSUM_TESTS)
  add_subdirectory(unit_tests)
  add_subdirectory(compile_fail)
endif ()

if (CHECKSUM_BENCHMARKS)
  add_subdirectory(benchmarks)
endif ()
