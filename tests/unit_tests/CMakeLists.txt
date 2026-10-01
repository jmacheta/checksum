add_executable(checksum_ut)

target_compile_features(checksum_ut PRIVATE cxx_std_23)

target_sources(checksum_ut PRIVATE ut_crc_catalog.cpp ut_crc_engine.cpp ut_crc_parameters.cpp ut_crc_polynomial.cpp)

# Expose internal headers to test internal functionality.
get_target_property(private_headers_dirs checksum HEADER_DIRS_private_headers)
target_include_directories(checksum_ut PRIVATE ${private_headers_dirs})

target_link_libraries(checksum_ut PRIVATE checksum::checksum GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(checksum_ut)
