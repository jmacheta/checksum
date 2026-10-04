include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

if (CHECKSUM_TESTS)
  CPMAddPackage("gh:google/googletest@1.17.0")
  target_compile_options(gtest PUBLIC $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-null-dereference>)
endif ()

if (CHECKSUM_BENCHMARKS)
  CPMAddPackage(
    NAME benchmark GITHUB_REPOSITORY google/benchmark VERSION 1.9.5 OPTIONS "BENCHMARK_ENABLE_TESTING OFF" "BENCHMARK_ENABLE_GTEST_TESTS OFF"
                                                                            "BENCHMARK_ENABLE_INSTALL OFF" "BENCHMARK_ENABLE_WERROR OFF"
  )
  # Silences the project warning flags (CXXFLAGS) in third-party sources.
  target_compile_options(benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)
  # Benchmark baseline: zlib's crc32.c with its default braided C loop. Its only hardware path is ARMv8 CRC32.
  CPMAddPackage(NAME zlib GITHUB_REPOSITORY madler/zlib VERSION 1.3.2 DOWNLOAD_ONLY YES)
  add_library(checksum_bench_zlib_crc32 STATIC ${zlib_SOURCE_DIR}/crc32.c)
  target_include_directories(checksum_bench_zlib_crc32 SYSTEM PUBLIC ${zlib_SOURCE_DIR})
  target_compile_options(checksum_bench_zlib_crc32 PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-w>)
  # zlib's ARMv8 CRC32 path gives wrong CRCs on big-endian AArch64, so use its byte loop there.
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64_be")
    target_compile_definitions(checksum_bench_zlib_crc32 PRIVATE Z_TESTW=0)
  endif ()
endif ()
