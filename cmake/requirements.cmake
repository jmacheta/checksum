include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

if (CHECKSUM_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
  CPMAddPackage("gh:google/googletest@1.17.0")
  target_compile_options(gtest PUBLIC $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-null-dereference>)
  # GoogleTest builds itself with /WX under clang-cl, where it uses __int64 and -Wpedantic flags it.
  target_compile_options(gtest PRIVATE $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wno-language-extension-token>)
endif ()

# Bare metal (Generic) uses the stand-in in tests/benchmarks.
if (CHECKSUM_BENCHMARKS AND NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
  CPMAddPackage(
    NAME benchmark GITHUB_REPOSITORY google/benchmark VERSION 1.9.5 OPTIONS "BENCHMARK_ENABLE_TESTING OFF" "BENCHMARK_ENABLE_GTEST_TESTS OFF"
                                                                            "BENCHMARK_ENABLE_INSTALL OFF" "BENCHMARK_ENABLE_WERROR OFF"
  )
  # Silences the project warning flags (CXXFLAGS) in third-party sources.
  target_compile_options(benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)
endif ()
