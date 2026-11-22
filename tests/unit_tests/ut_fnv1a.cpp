// FNV-1a 32 and 64: reference values, splits, resumption and byte ranges.

#include <checksum/engine.hpp>
#include <checksum/fnv1a.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// The hashes of the first size bytes of fill_random() with seed 0, from an independent implementation of the FNV reference.
struct prefix_vector {
  std::size_t size;
  std::uint32_t hash32;
  std::uint64_t hash64;
};

constexpr std::array<prefix_vector, 14> prefix_vectors{{
    {0, 0x811C9DC5U, 0xCBF29CE484222325U},
    {1, 0xA00BBE20U, 0xAF64184C86025280U},
    {3, 0xF0A2C647U, 0xEC50011A297AA427U},
    {4, 0xAF3F110DU, 0x06969E747B65F08DU},
    {7, 0xEA793E11U, 0x397608F938E0E351U},
    {8, 0x54DCF228U, 0x8474B77BA62284E8U},
    {15, 0xFBD61147U, 0x9AFBD974D360E527U},
    {16, 0x9FFD0B6AU, 0xBAD7CF832DA5A38AU},
    {63, 0x9B5433B0U, 0x56102322C95D2270U},
    {64, 0xBC8C9F95U, 0x9A8EAB1C294269B5U},
    {65, 0x535F19A6U, 0xE2D662DA1BD97E46U},
    {100, 0x63802910U, 0x3C69D186417D8F90U},
    {255, 0x6E2C50B7U, 0xEF6F71D3D88678D7U},
    {1000, 0xE9BB2974U, 0x94C0EDD8B8EF9E14U},
}};

constexpr std::array<std::byte, 1000> message = [] {
  std::array<std::byte, 1000> result{};
  fill_random(result);
  return result;
}();

template <unsigned Width> constexpr std::conditional_t<Width == 32, std::uint32_t, std::uint64_t> expected(prefix_vector const &vector) {
  if constexpr(Width == 32) {
    return vector.hash32;
  } else {
    return vector.hash64;
  }
}

// Values of the FNV test suite.
static_assert(fnv1a_compute<32>(""sv) == 0x811C9DC5U && fnv1a_compute<64>(""sv) == 0xCBF29CE484222325U);
static_assert(fnv1a_compute<32>("a"sv) == 0xE40C292CU && fnv1a_compute<64>("a"sv) == 0xAF63DC4C8601EC8CU);
static_assert(fnv1a_compute<32>("foobar"sv) == 0xBF9CF968U && fnv1a_compute<64>("foobar"sv) == 0x85944171F73967E8U);
static_assert(fnv1a_compute<32>("chongo was here!\n"sv) == 0xD49930D5U && fnv1a_compute<64>("chongo was here!\n"sv) == 0x46810940EFF5F915U);
static_assert(fnv1a_compute<32>(std::span(message)) == prefix_vectors.back().hash32);
static_assert(fnv1a_compute<64>(std::span(message).first(100)) == prefix_vectors[11].hash64);
static_assert(engine_checks::chunked(fnv1a_engine<64>{}, message, 7) == prefix_vectors.back().hash64);
static_assert(engine_checks::continued(fnv1a_engine<32>{}, fnv1a_compute<32>("foo"sv), "bar"sv) == 0xBF9CF968U);
static_assert(fnv1a_engine<32>{}.initial() == fnv1a_state<32>{} && fnv1a_engine<64>{}.finalize(fnv1a_state<64>{}) == 0xCBF29CE484222325U);
static_assert(resumable_engine<fnv1a_engine<32>> && resumable_engine<fnv1a_dispatch_engine<64>>);
static_assert(fnv1a_dispatch_engine<64>(isa_set{}).preset() == isa::generic);

template <class Width> class fnv1a : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 64>>;
TYPED_TEST_SUITE(fnv1a, widths);

TYPED_TEST(fnv1a, reference_values) {
  constexpr unsigned width = TypeParam::value;
  for(prefix_vector const &vector : prefix_vectors) {
    auto const data = std::span(message).first(vector.size);
    EXPECT_EQ(fnv1a_compute<width>(data), expected<width>(vector)) << "size " << vector.size;
    EXPECT_EQ(fnv1a_dispatch_engine<width>{}.compute(data), expected<width>(vector)) << "size " << vector.size;
  }
  auto const long_message = random_bytes(std::size_t{1} << 20U, 5);
  EXPECT_EQ(fnv1a_compute<width>(long_message), width == 32 ? 0x3A1BB3ECU : 0xC1DD082B522332ACU);
}

TYPED_TEST(fnv1a, split_anywhere) {
  engine_checks::check_split_anywhere(fnv1a_engine<TypeParam::value>{}, std::span(message).first(100));
  static_assert(engine_checks::same_state_for_any_split(fnv1a_engine<TypeParam::value>{}, std::span(message).first(40)));
}

TYPED_TEST(fnv1a, resume_from_a_stored_hash) {
  constexpr unsigned width = TypeParam::value;
  fnv1a_engine<width> const engine;
  for(std::size_t const split : {0UZ, 1UZ, 64UZ, 999UZ, 1000UZ}) {
    auto const stored = engine.compute(std::span(message).first(split));
    EXPECT_EQ(engine.finalize(engine.update(engine.resume(stored), std::span(message).subspan(split))), expected<width>(prefix_vectors.back()))
        << "split " << split;
  }
}

TYPED_TEST(fnv1a, byte_ranges) {
  constexpr unsigned width = TypeParam::value;
  constexpr std::string_view text = "123456789 123456789 123456789 123456789";
  auto const expected_hash = fnv1a_compute<width>(std::as_bytes(std::span(text)));
  EXPECT_EQ(fnv1a_compute<width>(text), expected_hash);
  EXPECT_EQ(fnv1a_compute<width>(std::string(text)), expected_hash);
  EXPECT_EQ(fnv1a_compute<width>(std::list<char>(text.begin(), text.end())), expected_hash);
  EXPECT_EQ(fnv1a_compute<width>(std::vector<unsigned char>(text.begin(), text.end())), expected_hash);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(fnv1a_compute<width>(std::list<std::byte>(message.begin(), message.end())), expected<width>(prefix_vectors.back()));
}

} // namespace
