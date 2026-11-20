// MurmurHash3_x86_32 and MurmurHash3_x64_128: reference values, splits, alignments, ranges and edge values.

#include <checksum/engine.hpp>
#include <checksum/hash128.hpp>
#include <checksum/murmur3.hpp>
#include <murmur3_reference_vectors.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <algorithm>
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
using murmur3_test::murmur3_128_vectors;
using murmur3_test::murmur3_32_vectors;

constexpr std::array<std::byte, murmur3_test::prefix_count - 1> message = [] {
  std::array<std::byte, murmur3_test::prefix_count - 1> result{};
  fill_random(result);
  return result;
}();

template <unsigned Width> constexpr auto const &vectors() {
  if constexpr(Width == 32) {
    return murmur3_32_vectors;
  } else {
    return murmur3_128_vectors;
  }
}

// Number of prefixes of message up to size_limit bytes whose hash differs from the reference, for every seed.
template <unsigned Width> constexpr std::size_t prefix_mismatches(std::size_t size_limit) {
  std::size_t mismatches = 0;
  for(auto const &set : vectors<Width>()) {
    for(std::size_t size = 0; size <= size_limit; ++size) {
      mismatches += murmur3_compute<Width>(std::span(message).first(size), set.seed) != set.prefixes[size] ? 1U : 0U;
    }
  }
  return mismatches;
}

// The hash of data, folded in pieces of chunk_size bytes.
template <unsigned Width> constexpr auto chunked(std::span<std::byte const> data, std::size_t chunk_size, std::uint32_t seed) {
  return engine_checks::chunked(murmur3_engine<Width>(seed), data, chunk_size);
}

// Every tail length around a few blocks.
constexpr std::size_t constant_limit = 70;

using namespace std::literals;

static_assert(murmur3_compute<32>(""sv) == 0);
static_assert(murmur3_compute<32>(""sv, 1) == 0x514E28B7U);
static_assert(murmur3_compute<32>("Hello, world!"sv, 1234) == 0xFAF6CDB3U);
static_assert(murmur3_compute<128>(""sv) == hash128{});
static_assert(murmur3_compute<128>("Hello, world!"sv, 1234) == hash128{.low = 0x61130E64AA0AC6FEU, .high = 0x51F9046D087E1B56U});
static_assert(murmur3_engine<32>{1}.finalize(murmur3_state<32>{}) == 0x514E28B7U);
static_assert(murmur3_compute<32>(std::span(message), murmur3_32_vectors[2].seed) == murmur3_32_vectors[2].prefixes[300]);
static_assert(murmur3_compute<128>(std::span(message).first(33), murmur3_128_vectors[2].seed) == murmur3_128_vectors[2].prefixes[33]);
static_assert(chunked<128>(message, 7, murmur3_128_vectors[1].seed) == murmur3_128_vectors[1].prefixes[300]);
static_assert(chunked<32>(message, 3, murmur3_32_vectors[1].seed) == murmur3_32_vectors[1].prefixes[300]);
static_assert(murmur3_engine<32>{1234}.compute("Hello, world!"sv) == 0xFAF6CDB3U);
static_assert(engine<murmur3_engine<32>> && engine<murmur3_dispatch_engine<128>> && !resumable_engine<murmur3_engine<128>>);

template <class Width> class murmur3 : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 128>>;
// Names the test instances x86_32 and x64_128.
struct width_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Width> static std::string GetName(int /*index*/) { return Width::value == 32 ? "x86_32" : "x64_128"; }
};

TYPED_TEST_SUITE(murmur3, widths, width_names);

TYPED_TEST(murmur3, reference_values) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t size = 0; size < murmur3_test::prefix_count; ++size) {
      auto const data = std::span(message).first(size);
      ASSERT_EQ(murmur3_compute<width>(data, set.seed), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
      murmur3_engine<width> const engine(set.seed);
      ASSERT_EQ(engine.finalize(engine.update(engine.initial(), data)), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
    }
  }
}

TYPED_TEST(murmur3, every_size_and_offset) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[2];
  for(std::size_t offset = 0; offset < 16; ++offset) {
    std::vector<std::byte> buffer(offset + message.size());
    std::ranges::copy(message, buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    for(std::size_t size = 0; size < murmur3_test::prefix_count; ++size) {
      ASSERT_EQ(murmur3_compute<width>(std::span(buffer).subspan(offset, size), set.seed), set.prefixes[size])
          << "offset " << offset << ", size " << size;
    }
  }
}

TYPED_TEST(murmur3, split_anywhere) {
  constexpr unsigned width = TypeParam::value;
  constexpr std::size_t size = 100;
  auto const &set = vectors<width>()[2];
  auto const data = std::span<std::byte const>(message).first(size);
  for(std::size_t first = 0; first <= size; ++first) {
    for(std::size_t second = first; second <= size; ++second) {
      murmur3_engine<width> const engine(set.seed);
      murmur3_state<width> state = engine.update(engine.initial(), data.first(first));
      state = engine.update(state, data.subspan(first, second - first));
      ASSERT_EQ(engine.finalize(state), set.prefixes[second]) << "split at " << first << " and " << second;
      ASSERT_EQ(engine.finalize(engine.update(state, data.subspan(second))), set.prefixes[size]) << "split at " << first << " and " << second;
    }
  }
}

// Pieces of every size up to past two 32-byte spans, so that splits fall at every offset around the 4- and 16-byte block boundaries.
TYPED_TEST(murmur3, split_into_chunks) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t chunk_size = 1; chunk_size <= 70; ++chunk_size) {
      for(std::size_t const size : {3U, 4U, 5U, 15U, 16U, 17U, 31U, 32U, 33U, 47U, 48U, 49U, 63U, 64U, 65U, 300U}) {
        ASSERT_EQ(chunked<width>(std::span(message).first(size), chunk_size, set.seed), set.prefixes[size])
            << "chunk " << chunk_size << ", size " << size;
      }
    }
  }
}

TYPED_TEST(murmur3, byte_ranges) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[1];
  constexpr std::string_view text = "123456789 123456789 123456789 123456789";
  auto const expected = murmur3_compute<width>(std::as_bytes(std::span(text)), set.seed);
  EXPECT_EQ(murmur3_compute<width>(text, set.seed), expected);
  EXPECT_EQ(murmur3_compute<width>(std::list<char>(text.begin(), text.end()), set.seed), expected);
  EXPECT_EQ(murmur3_compute<width>(std::vector<unsigned char>(text.begin(), text.end()), set.seed), expected);
  murmur3_engine<width> const engine(set.seed);
  EXPECT_EQ(engine.finalize(engine.update(engine.initial(), text)), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(murmur3_compute<width>(std::list<std::byte>(message.begin(), message.end()), set.seed), set.prefixes[300]);
}

// Before the first block, the lanes and the buffer bytes past the message are unused and may hold anything; width 32 mixes in the
// length modulo 2^32.
TYPED_TEST(murmur3, edge_values) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[2];
  for(std::size_t size = 0; size < width / 8; ++size) {
    murmur3_state<width> state{.length = size};
    state.lanes.fill(0xA5A5A5A5U);
    state.buffer.fill(std::byte{0x5A});
    std::ranges::copy(std::span(message).first(size), state.buffer.begin());
    EXPECT_EQ(murmur3_engine<width>{set.seed}.finalize(state), set.prefixes[size]) << "size " << size;
  }
  if constexpr(width == 32) {
    murmur3_state<32> const state{.lanes = {0x12345678U}, .length = (std::uint64_t{1} << 32U) + 5, .buffer = {std::byte{0x42}}};
    murmur3_state<32> wrapped = state;
    wrapped.length = 5;
    EXPECT_EQ(murmur3_engine<32>{}.finalize(state), murmur3_engine<32>{}.finalize(wrapped));
  }
}

// States of one message compare equal however it was split, though their buffers keep different stale bytes.
TYPED_TEST(murmur3, equal_states_after_any_split) {
  constexpr unsigned width = TypeParam::value;
  static_assert(engine_checks::same_state_for_any_split(murmur3_engine<width>(7), std::span(message).first(70)));
  for(std::size_t const size : {0UZ, 1UZ, 3UZ, 4UZ, 5UZ, 15UZ, 16UZ, 17UZ, 33UZ, 300UZ}) {
    EXPECT_TRUE(engine_checks::same_state_for_any_split(murmur3_engine<width>(7), std::span(message).first(size))) << "size " << size;
  }
}

// Every prefix up to past a few blocks computed at compile time, for every seed; longer prefixes exceed the constant-evaluation step limit
// of Clang.
TYPED_TEST(murmur3, constant_evaluation) {
  static constexpr unsigned width = TypeParam::value;
  static_assert(prefix_mismatches<width>(constant_limit) == 0);
  static constexpr auto prefixes = [] {
    std::array<std::array<typename std::remove_cvref_t<decltype(vectors<width>()[0].prefixes)>::value_type, constant_limit + 1>, 3> result{};
    for(std::size_t set = 0; set < result.size(); ++set) {
      for(std::size_t size = 0; size <= constant_limit; ++size) {
        result[set][size] = murmur3_compute<width>(std::span(message).first(size), vectors<width>()[set].seed);
      }
    }
    return result;
  }();
  for(std::size_t set = 0; set < prefixes.size(); ++set) {
    for(std::size_t size = 0; size <= constant_limit; ++size) {
      ASSERT_EQ(prefixes[set][size], murmur3_compute<width>(std::span(message).first(size), vectors<width>()[set].seed)) << "size " << size;
    }
  }
}

TYPED_TEST(murmur3, long_message) {
  constexpr unsigned width = TypeParam::value;
  std::vector<std::byte> data(murmur3_test::long_message_size);
  fill_random(data);
  for(auto const &set : vectors<width>()) {
    EXPECT_EQ(murmur3_compute<width>(data, set.seed), set.long_message) << "seed " << set.seed;
    EXPECT_EQ(chunked<width>(data, 4099, set.seed), set.long_message) << "seed " << set.seed;
  }
}

} // namespace
