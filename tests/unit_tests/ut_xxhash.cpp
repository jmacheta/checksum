// XXH32 and XXH64: reference values, splits, alignments and ranges.

#include <checksum/xxhash.hpp>
#include <xxhash_reference_vectors.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <algorithm>
#include <array>
#include <concepts>
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
using xxhash_test::xxhash32_vectors;
using xxhash_test::xxhash64_vectors;

constexpr std::array<std::byte, xxhash_test::prefix_count - 1> message = [] {
  std::array<std::byte, xxhash_test::prefix_count - 1> result{};
  fill_random(result);
  return result;
}();

template <unsigned Width> constexpr auto const &vectors() {
  if constexpr(Width == 32) {
    return xxhash32_vectors;
  } else {
    return xxhash64_vectors;
  }
}

// Number of prefixes of message up to size_limit bytes whose hash differs from the reference, for every seed.
template <unsigned Width> constexpr std::size_t prefix_mismatches(std::size_t size_limit) {
  std::size_t mismatches = 0;
  for(auto const &set : vectors<Width>()) {
    for(std::size_t size = 0; size <= size_limit; ++size) {
      mismatches += xxhash_compute<Width>(std::span(message).first(size), set.seed) != set.prefixes[size] ? 1U : 0U;
    }
  }
  return mismatches;
}

// The hash of data, folded in pieces of chunk_size bytes.
template <unsigned Width>
constexpr auto chunked(std::span<std::byte const> data, std::size_t chunk_size, typename xxhash_state<Width>::value_type seed) {
  xxhash_state<Width> state{.seed = seed};
  for(; data.size() > chunk_size; data = data.subspan(chunk_size)) {
    state = xxhash_update(state, data.first(chunk_size));
  }
  return xxhash_finalize(xxhash_update(state, data));
}

using namespace std::literals;

static_assert(xxhash_compute<32>(""sv) == 0x02CC5D05U);
static_assert(xxhash_compute<64>(""sv) == 0xEF46DB3751D8E999U);
static_assert(xxhash_compute<32>("abc"sv) == 0x32D153FFU);
static_assert(xxhash_compute<64>("abc"sv) == 0x44BC2CF5AD770999U);
static_assert(xxhash_finalize(xxhash_state<64>{}) == 0xEF46DB3751D8E999U);
static_assert(xxhash_compute<32>(std::span(message), xxhash32_vectors[2].seed) == xxhash32_vectors[2].prefixes[300]);
static_assert(xxhash_compute<64>(std::span(message).first(33), xxhash64_vectors[2].seed) == xxhash64_vectors[2].prefixes[33]);
static_assert(chunked<64>(message, 7, xxhash64_vectors[1].seed) == xxhash64_vectors[1].prefixes[300]);
static_assert(chunked<32>(message, 13, xxhash32_vectors[1].seed) == xxhash32_vectors[1].prefixes[300]);
// Every tail length around two XXH64 stripes; longer prefixes exceed the constant-evaluation step limit of Clang.
static_assert(prefix_mismatches<32>(70) == 0);
static_assert(prefix_mismatches<64>(70) == 0);
static_assert(std::same_as<xxh32_state, xxhash_state<32>> && std::same_as<xxh64_state, xxhash_state<64>>);
static_assert(xxh32_compute("abc"sv) == 0x32D153FFU && xxh64_compute("abc"sv, 1) == xxhash_compute<64>("abc"sv, 1));

template <class Width> class xxhash : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 64>>;
// Names the test instances xxh32 and xxh64.
struct width_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Width> static std::string GetName(int /*index*/) { return "xxh" + std::to_string(Width::value); }
};

TYPED_TEST_SUITE(xxhash, widths, width_names);

TYPED_TEST(xxhash, reference_prefixes) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t size = 0; size < xxhash_test::prefix_count; ++size) {
      auto const data = std::span(message).first(size);
      ASSERT_EQ(xxhash_compute<width>(data, set.seed), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
      ASSERT_EQ(xxhash_finalize(xxhash_update(xxhash_state<width>{.seed = set.seed}, data)), set.prefixes[size])
          << "seed " << set.seed << ", size " << size;
    }
  }
}

TYPED_TEST(xxhash, unaligned_starts) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[2];
  for(std::size_t offset = 0; offset < 16; ++offset) {
    std::vector<std::byte> buffer(offset + message.size());
    std::ranges::copy(message, buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    for(std::size_t size = 0; size < xxhash_test::prefix_count; ++size) {
      ASSERT_EQ(xxhash_compute<width>(std::span(buffer).subspan(offset, size), set.seed), set.prefixes[size])
          << "offset " << offset << ", size " << size;
    }
  }
}

TYPED_TEST(xxhash, split_anywhere) {
  constexpr unsigned width = TypeParam::value;
  constexpr std::size_t size = 100;
  auto const &set = vectors<width>()[2];
  auto const data = std::span<std::byte const>(message).first(size);
  for(std::size_t first = 0; first <= size; ++first) {
    for(std::size_t second = first; second <= size; ++second) {
      xxhash_state<width> state = xxhash_update(xxhash_state<width>{.seed = set.seed}, data.first(first));
      state = xxhash_update(state, data.subspan(first, second - first));
      ASSERT_EQ(xxhash_finalize(state), set.prefixes[second]) << "split at " << first << " and " << second;
      ASSERT_EQ(xxhash_finalize(xxhash_update(state, data.subspan(second))), set.prefixes[size]) << "split at " << first << " and " << second;
    }
  }
}

// Pieces of every size up to past two 32-byte stripes, so that splits fall at every offset around the stripe boundaries.
TYPED_TEST(xxhash, chunks_across_stripes) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t chunk_size = 1; chunk_size <= 70; ++chunk_size) {
      for(std::size_t const size : {15U, 16U, 17U, 31U, 32U, 33U, 47U, 48U, 49U, 63U, 64U, 65U, 300U}) {
        ASSERT_EQ(chunked<width>(std::span(message).first(size), chunk_size, set.seed), set.prefixes[size])
            << "chunk " << chunk_size << ", size " << size;
      }
    }
  }
}

TYPED_TEST(xxhash, byte_ranges) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[1];
  constexpr std::string_view text = "123456789 123456789 123456789 123456789";
  auto const expected = xxhash_compute<width>(std::as_bytes(std::span(text)), set.seed);
  EXPECT_EQ(xxhash_compute<width>(text, set.seed), expected);
  EXPECT_EQ(xxhash_compute<width>(std::list<char>(text.begin(), text.end()), set.seed), expected);
  EXPECT_EQ(xxhash_compute<width>(std::vector<unsigned char>(text.begin(), text.end()), set.seed), expected);
  EXPECT_EQ(xxhash_finalize(xxhash_update(xxhash_state<width>{.seed = set.seed}, text)), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(xxhash_compute<width>(std::list<std::byte>(message.begin(), message.end()), set.seed), set.prefixes[300]);
}

TYPED_TEST(xxhash, long_message) {
  constexpr unsigned width = TypeParam::value;
  std::vector<std::byte> data(xxhash_test::long_message_size);
  fill_random(data);
  for(auto const &set : vectors<width>()) {
    EXPECT_EQ(xxhash_compute<width>(data, set.seed), set.long_message) << "seed " << set.seed;
    EXPECT_EQ(chunked<width>(data, 4099, set.seed), set.long_message) << "seed " << set.seed;
  }
}

TEST(xxhash, aliases) {
  constexpr std::string_view text = "123456789 123456789 123456789 123456789";
  auto const bytes = std::as_bytes(std::span(text));
  for(auto const &set : xxhash32_vectors) {
    EXPECT_EQ(xxh32_compute(text, set.seed), xxhash_compute<32>(text, set.seed));
    EXPECT_EQ(xxh32_compute(bytes, set.seed), xxhash_compute<32>(bytes, set.seed));
    EXPECT_EQ(xxh32_compute(std::span(message), set.seed), set.prefixes[300]);
  }
  for(auto const &set : xxhash64_vectors) {
    EXPECT_EQ(xxh64_compute(text, set.seed), xxhash_compute<64>(text, set.seed));
    EXPECT_EQ(xxh64_compute(bytes, set.seed), xxhash_compute<64>(bytes, set.seed));
    EXPECT_EQ(xxh64_compute(std::span(message), set.seed), set.prefixes[300]);
  }
  EXPECT_EQ(xxhash_finalize(xxhash_update(xxh64_state{.seed = 7}, text)), xxh64_compute(text, 7));
}

// Seeds of all ones wrap every lane's initial value; hashes from xxhash.h 0.8.4.
TEST(xxhash, all_ones_seed) {
  struct expected_hashes {
    std::size_t size;
    std::uint32_t xxh32;
    std::uint64_t xxh64;
  };
  constexpr std::array<expected_hashes, 9> expected{{{0, 0x9061DA9DU, 0x298F4C84B24F5380U},
                                                     {1, 0xC69BCB3AU, 0x88E1E5E6C9D685D0U},
                                                     {15, 0xA39D9FA4U, 0x877DDBE62F7884ECU},
                                                     {16, 0x3048FD14U, 0x53DECA0A5E9FA4EEU},
                                                     {31, 0xE991E9D2U, 0x05A4D1DA768C4435U},
                                                     {32, 0x6B7A0148U, 0x4E2BDC2F1263D056U},
                                                     {33, 0xE0D1E0E3U, 0xCD076AEE4ECC8F1FU},
                                                     {64, 0xED7B1FE3U, 0x532DF1276DD21320U},
                                                     {300, 0x0CB5B0CAU, 0x536EB91EDFE26C25U}}};
  for(auto const &hashes : expected) {
    auto const data = std::span(message).first(hashes.size);
    EXPECT_EQ(xxhash_compute<32>(data, 0xFFFFFFFFU), hashes.xxh32) << "size " << hashes.size;
    EXPECT_EQ(xxhash_compute<64>(data, ~std::uint64_t{0}), hashes.xxh64) << "size " << hashes.size;
    EXPECT_EQ(chunked<32>(data, 7, 0xFFFFFFFFU), hashes.xxh32) << "size " << hashes.size;
    EXPECT_EQ(chunked<64>(data, 7, ~std::uint64_t{0}), hashes.xxh64) << "size " << hashes.size;
  }
}

} // namespace
