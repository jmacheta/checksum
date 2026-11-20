// XXH32 and XXH64: reference values, splits, alignments, ranges and edge seeds.

#include <checksum/xxhash.hpp>
#include <xxhash_reference_vectors.hpp>

#include <engine_checks.hpp>
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
constexpr auto chunked(std::span<std::byte const> data, std::size_t chunk_size, typename xxhash_engine<Width>::value_type seed) {
  return engine_checks::chunked(xxhash_engine<Width>(seed), data, chunk_size);
}

// Every tail length around two XXH64 stripes.
constexpr std::size_t constant_limit = 70;

using namespace std::literals;

static_assert(xxhash_compute<32>(""sv) == 0x02CC5D05U);
static_assert(xxhash_compute<64>(""sv) == 0xEF46DB3751D8E999U);
static_assert(xxhash_compute<32>("abc"sv) == 0x32D153FFU);
static_assert(xxhash_compute<64>("abc"sv) == 0x44BC2CF5AD770999U);
static_assert(xxhash_engine<64>{}.finalize(xxhash_state<64>{}) == 0xEF46DB3751D8E999U);
static_assert(xxhash_compute<32>(std::span(message), xxhash32_vectors[2].seed) == xxhash32_vectors[2].prefixes[300]);
static_assert(xxhash_compute<64>(std::span(message).first(33), xxhash64_vectors[2].seed) == xxhash64_vectors[2].prefixes[33]);
static_assert(chunked<64>(message, 7, xxhash64_vectors[1].seed) == xxhash64_vectors[1].prefixes[300]);
static_assert(chunked<32>(message, 13, xxhash32_vectors[1].seed) == xxhash32_vectors[1].prefixes[300]);
static_assert(xxhash_engine<64>{1}.compute("abc"sv) == xxhash_compute<64>("abc"sv, 1));
static_assert(engine<xxhash_engine<32>> && engine<xxhash_dispatch_engine<64>> && !resumable_engine<xxhash_engine<64>>);

template <class Width> class xxhash : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 64>>;
// Names the test instances xxh32 and xxh64.
struct width_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Width> static std::string GetName(int /*index*/) { return "xxh" + std::to_string(Width::value); }
};

TYPED_TEST_SUITE(xxhash, widths, width_names);

TYPED_TEST(xxhash, reference_values) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t size = 0; size < xxhash_test::prefix_count; ++size) {
      auto const data = std::span(message).first(size);
      ASSERT_EQ(xxhash_compute<width>(data, set.seed), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
      xxhash_engine<width> const engine(set.seed);
      ASSERT_EQ(engine.finalize(engine.update(engine.initial(), data)), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
    }
  }
}

TYPED_TEST(xxhash, every_size_and_offset) {
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
      xxhash_engine<width> const engine(set.seed);
      xxhash_state<width> state = engine.update(engine.initial(), data.first(first));
      state = engine.update(state, data.subspan(first, second - first));
      ASSERT_EQ(engine.finalize(state), set.prefixes[second]) << "split at " << first << " and " << second;
      ASSERT_EQ(engine.finalize(engine.update(state, data.subspan(second))), set.prefixes[size]) << "split at " << first << " and " << second;
    }
  }
}

// Pieces of every size up to past two 32-byte stripes, so that splits fall at every offset around the stripe boundaries.
TYPED_TEST(xxhash, split_into_chunks) {
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
  xxhash_engine<width> const engine(set.seed);
  EXPECT_EQ(engine.finalize(engine.update(engine.initial(), text)), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(xxhash_compute<width>(std::list<std::byte>(message.begin(), message.end()), set.seed), set.prefixes[300]);
}

// States of one message compare equal however it was split, though their buffers keep different stale bytes.
TYPED_TEST(xxhash, equal_states_after_any_split) {
  constexpr unsigned width = TypeParam::value;
  static_assert(engine_checks::same_state_for_any_split(xxhash_engine<width>(7), std::span(message).first(70)));
  for(std::size_t const size : {0UZ, 1UZ, 15UZ, 16UZ, 17UZ, 31UZ, 32UZ, 33UZ, 65UZ, 300UZ}) {
    EXPECT_TRUE(engine_checks::same_state_for_any_split(xxhash_engine<width>(7), std::span(message).first(size))) << "size " << size;
  }
}

// Every prefix up to past a few blocks computed at compile time, for every seed; longer prefixes exceed the constant-evaluation step limit
// of Clang.
TYPED_TEST(xxhash, constant_evaluation) {
  static constexpr unsigned width = TypeParam::value;
  static_assert(prefix_mismatches<width>(constant_limit) == 0);
  static constexpr auto prefixes = [] {
    std::array<std::array<typename std::remove_cvref_t<decltype(vectors<width>()[0].prefixes)>::value_type, constant_limit + 1>, 3> result{};
    for(std::size_t set = 0; set < result.size(); ++set) {
      for(std::size_t size = 0; size <= constant_limit; ++size) {
        result[set][size] = xxhash_compute<width>(std::span(message).first(size), vectors<width>()[set].seed);
      }
    }
    return result;
  }();
  for(std::size_t set = 0; set < prefixes.size(); ++set) {
    for(std::size_t size = 0; size <= constant_limit; ++size) {
      ASSERT_EQ(prefixes[set][size], xxhash_compute<width>(std::span(message).first(size), vectors<width>()[set].seed)) << "size " << size;
    }
  }
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

// Seeds of all ones wrap every lane's initial value; hashes from xxhash.h 0.8.4.
TYPED_TEST(xxhash, edge_values) {
  constexpr unsigned width = TypeParam::value;
  constexpr auto seed = static_cast<xxhash_engine<width>::value_type>(~std::uint64_t{0});
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
    auto const expected = width == 32 ? hashes.xxh32 : hashes.xxh64;
    EXPECT_EQ(xxhash_compute<width>(data, seed), expected) << "size " << hashes.size;
    EXPECT_EQ(chunked<width>(data, 7, seed), expected) << "size " << hashes.size;
  }
}

} // namespace
