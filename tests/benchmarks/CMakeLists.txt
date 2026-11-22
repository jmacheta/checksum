# One executable checksum_bench_<algorithm> per algorithm, each with a smoke test that runs one tiny benchmark. Measurements use the native-*-bench presets, or bare-cortex-* on a board.
#
# Bare metal (Generic) links a stand-in for Google Benchmark and startup.S instead, and runs the smoke tests
# in QEMU system mode, which passes the program's arguments through -append.
if (CMAKE_SYSTEM_NAME STREQUAL "Generic")
  enable_language(ASM)
  configure_file(bare_metal_benchmark.hpp include/benchmark/benchmark.h COPYONLY)
  add_library(checksum_bench_bare_metal OBJECT bare_metal_benchmark.cpp startup.S)
  target_compile_features(checksum_bench_bare_metal PUBLIC cxx_std_23)
  target_include_directories(checksum_bench_bare_metal PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src/include_private)
  target_link_libraries(checksum_bench_bare_metal PRIVATE checksum::checksum)
  set(benchmark_library checksum_bench_bare_metal)
  set(argument_option -append)
else ()
  set(benchmark_library benchmark::benchmark)
  set(argument_option)
endif ()

foreach (
  algorithm IN
  ITEMS crc
        internet
        fletcher
        fletcher4
        adler32
        xxhash
        xxh3
        murmur3
        fnv1a
)
  add_executable(checksum_bench_${algorithm} bench_${algorithm}.cpp)
  target_compile_features(checksum_bench_${algorithm} PRIVATE cxx_std_23)
  target_link_libraries(checksum_bench_${algorithm} PRIVATE checksum::checksum checksum_test_data ${benchmark_library})
endforeach ()

if (CMAKE_SYSTEM_NAME STREQUAL "Generic" AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
  return()
endif ()
foreach (
  smoke IN
  ITEMS "crc ^byte/crc32_iso_hdlc/16$"
        "internet ^internet/20$"
        "fletcher ^fletcher16/20/0$"
        "fletcher4 ^fletcher4/20$"
        "adler32 ^adler32/20/0$"
        "xxhash ^xxhash64/20$"
        "xxh3 ^xxh3_128/20$"
        "murmur3 ^murmur3_128/20$"
        "fnv1a ^fnv1a_64/20$"
)
  separate_arguments(smoke)
  list(GET smoke 0 algorithm)
  list(GET smoke 1 filter)
  set(arguments "--benchmark_filter=${filter} --benchmark_min_time=0.001s")
  if (NOT argument_option)
    separate_arguments(arguments)
  endif ()
  add_test(NAME benchmark_${algorithm}_smoke COMMAND checksum_bench_${algorithm} ${argument_option} ${arguments})
  set_tests_properties(benchmark_${algorithm}_smoke PROPERTIES TIMEOUT 300)
endforeach ()
