add_executable(checksum_ut)

target_compile_features(checksum_ut PRIVATE cxx_std_23)

target_sources(
  checksum_ut
  PRIVATE ut_adler32.cpp
          ut_crc_acceleration.cpp
          ut_crc_accumulator.cpp
          ut_crc_catalog.cpp
          ut_crc_engine.cpp
          ut_crc_parameters.cpp
          ut_crc_polynomial.cpp
          ut_crc_vectors.cpp
          ut_fletcher.cpp
          ut_internet_checksum.cpp
          ut_murmur3.cpp
          ut_xxhash.cpp
  PRIVATE FILE_SET
          HEADERS
          BASE_DIRS
          include
          FILES
          include/crc_catalog_checks.hpp
          include/crc_reference_vectors.hpp
          include/crc_test_support.hpp
          include/murmur3_reference_vectors.hpp
          include/xxhash_reference_vectors.hpp
)

target_link_libraries(checksum_ut PRIVATE checksum::checksum GTest::gtest_main)

# Built with NDEBUG to test behavior with precondition checks disabled.
add_executable(checksum_ut_unchecked)
target_compile_features(checksum_ut_unchecked PRIVATE cxx_std_23)
target_sources(checksum_ut_unchecked PRIVATE ut_crc_unchecked.cpp PRIVATE FILE_SET HEADERS BASE_DIRS include FILES include/crc_test_support.hpp)
target_compile_definitions(checksum_ut_unchecked PRIVATE NDEBUG)
target_link_libraries(checksum_ut_unchecked PRIVATE checksum::checksum GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(checksum_ut)
gtest_discover_tests(checksum_ut_unchecked)
