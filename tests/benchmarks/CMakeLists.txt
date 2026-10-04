add_executable(checksum_bench bench_crc.cpp)
target_compile_features(checksum_bench PRIVATE cxx_std_23)
target_link_libraries(checksum_bench PRIVATE checksum::checksum checksum_test_data benchmark::benchmark checksum_bench_zlib_crc32)

# Smoke test: checks the baselines against the library and runs one tiny benchmark. Measurements use the native-*-bench presets.
add_test(NAME benchmark_smoke COMMAND checksum_bench --benchmark_filter=^byte/crc32_iso_hdlc/16$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_internet bench_internet.cpp)
target_compile_features(checksum_bench_internet PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_internet PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_internet_smoke COMMAND checksum_bench_internet --benchmark_filter=^internet/20$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_fletcher bench_fletcher.cpp)
target_compile_features(checksum_bench_fletcher PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_fletcher PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_fletcher_smoke COMMAND checksum_bench_fletcher --benchmark_filter=^fletcher<16>/20/0$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_fletcher4 bench_fletcher4.cpp)
target_compile_features(checksum_bench_fletcher4 PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_fletcher4 PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_fletcher4_smoke COMMAND checksum_bench_fletcher4 --benchmark_filter=^fletcher4/20$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_adler32 bench_adler32.cpp)
target_compile_features(checksum_bench_adler32 PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_adler32 PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_adler32_smoke COMMAND checksum_bench_adler32 --benchmark_filter=^adler32/20/0$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_xxhash bench_xxhash.cpp)
target_compile_features(checksum_bench_xxhash PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_xxhash PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_xxhash_smoke COMMAND checksum_bench_xxhash --benchmark_filter=^xxhash64/20$ --benchmark_min_time=0.001s)

add_executable(checksum_bench_murmur3 bench_murmur3.cpp)
target_compile_features(checksum_bench_murmur3 PRIVATE cxx_std_23)
target_link_libraries(checksum_bench_murmur3 PRIVATE checksum::checksum checksum_test_data benchmark::benchmark)
add_test(NAME benchmark_murmur3_smoke COMMAND checksum_bench_murmur3 --benchmark_filter=^murmur3_128/20$ --benchmark_min_time=0.001s)
add_test(NAME benchmark_xxh3_smoke COMMAND checksum_bench_xxhash --benchmark_filter=^xxh3_128/20$ --benchmark_min_time=0.001s)
