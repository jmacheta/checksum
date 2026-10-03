add_executable(checksum_bench bench_crc.cpp)
target_compile_features(checksum_bench PRIVATE cxx_std_23)
target_link_libraries(checksum_bench PRIVATE checksum::checksum benchmark::benchmark checksum_bench_zlib_crc32)

# Smoke test: checks the baselines against the library and runs one tiny benchmark. Measurements use the native-*-bench presets.
add_test(NAME benchmark_smoke COMMAND checksum_bench --benchmark_filter=^byte/crc32_iso_hdlc/16$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_internet bench_internet.cpp)
target_compile_features(checksum_bench_internet PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_internet PRIVATE checksum::checksum benchmark::benchmark)
add_test(NAME benchmark_internet_smoke COMMAND checksum_bench_internet --benchmark_filter=^internet/20$ --benchmark_min_time=0.001s)
