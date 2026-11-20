# One executable checksum_bench_<algorithm> per algorithm, each with a smoke test that runs one tiny benchmark (CRC's also
# checks its baselines against the library). Measurements use the native-*-bench presets.
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
)
  add_executable(checksum_bench_${algorithm} bench_${algorithm}.cpp)
  target_compile_features(checksum_bench_${algorithm} PRIVATE cxx_std_23)
  target_link_libraries(checksum_bench_${algorithm} PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
endforeach ()
target_link_libraries(checksum_bench_crc PRIVATE checksum_bench_zlib_crc32)

add_test(NAME benchmark_crc_smoke COMMAND checksum_bench_crc --benchmark_filter=^byte/crc32_iso_hdlc/16$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_internet_smoke COMMAND checksum_bench_internet --benchmark_filter=^internet/20$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_fletcher_smoke COMMAND checksum_bench_fletcher --benchmark_filter=^fletcher16/20/0$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_fletcher4_smoke COMMAND checksum_bench_fletcher4 --benchmark_filter=^fletcher4/20$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_adler32_smoke COMMAND checksum_bench_adler32 --benchmark_filter=^adler32/20/0$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_xxhash_smoke COMMAND checksum_bench_xxhash --benchmark_filter=^xxhash64/20$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_xxh3_smoke COMMAND checksum_bench_xxh3 --benchmark_filter=^xxh3_128/20$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_murmur3_smoke COMMAND checksum_bench_murmur3 --benchmark_filter=^murmur3_128/20$ --benchmark_min_time=0.001s)
