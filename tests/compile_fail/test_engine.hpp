#ifndef CHECKSUM_TESTS_COMPILE_FAIL_TEST_ENGINE_HPP
#define CHECKSUM_TESTS_COMPILE_FAIL_TEST_ENGINE_HPP

// The engine of the algorithm that CHECKSUM_TEST_<ALGORITHM> selects, so that one negative test source covers every algorithm.

#include <checksum/isa.hpp>

#if defined(CHECKSUM_TEST_ADLER32)
#include <checksum/adler32.hpp>
template <checksum::isa Isa> using test_engine = checksum::adler32_engine<Isa>;
#elif defined(CHECKSUM_TEST_CRC)
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include <cstdint>
template <checksum::isa Isa> using test_engine = checksum::crc_engine<std::uint32_t, checksum::crc_lut_sliced, Isa>;
#elif defined(CHECKSUM_TEST_FLETCHER)
#include <checksum/fletcher.hpp>
template <checksum::isa Isa> using test_engine = checksum::fletcher_engine<32, Isa>;
#elif defined(CHECKSUM_TEST_FLETCHER4)
#include <checksum/fletcher4.hpp>
template <checksum::isa Isa> using test_engine = checksum::fletcher4_engine<Isa>;
#elif defined(CHECKSUM_TEST_INTERNET)
#include <checksum/internet.hpp>
template <checksum::isa Isa> using test_engine = checksum::internet_engine<Isa>;
#elif defined(CHECKSUM_TEST_MURMUR3)
#include <checksum/murmur3.hpp>
template <checksum::isa Isa> using test_engine = checksum::murmur3_engine<32, Isa>;
#elif defined(CHECKSUM_TEST_XXH3)
#include <checksum/xxh3.hpp>
template <checksum::isa Isa> using test_engine = checksum::xxh3_engine<64, Isa>;
#elif defined(CHECKSUM_TEST_XXHASH)
#include <checksum/xxhash.hpp>
template <checksum::isa Isa> using test_engine = checksum::xxhash_engine<64, Isa>;
#else
#error "define CHECKSUM_TEST_<ALGORITHM>"
#endif

// A test_engine of preset Isa: CRC-32C for CRC, a default-constructed engine otherwise. The deduced return type keeps an unavailable
// preset a constraint failure of the engine rather than a candidate removed from overload resolution.
template <checksum::isa Isa> constexpr auto make_engine() {
#if defined(CHECKSUM_TEST_CRC)
  return *test_engine<Isa>::create(checksum::crc32c);
#else
  return test_engine<Isa>{};
#endif
}

#endif // CHECKSUM_TESTS_COMPILE_FAIL_TEST_ENGINE_HPP
