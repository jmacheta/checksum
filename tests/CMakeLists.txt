if (CHECKSUM_TESTS)
  add_subdirectory(unit_tests)
  add_subdirectory(compile_fail)
endif ()

if (CHECKSUM_BENCHMARKS)
  add_subdirectory(benchmarks)
endif ()
