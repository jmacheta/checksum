// MurmurHash3_x86_32 and MurmurHash3_x64_128: reference values, splits, alignments and ranges.

#include <checksum/murmur3.hpp>
#include <murmur3_reference_vectors.hpp>

#include <gtest/gtest.h>

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
using murmur3_test::hash128;
using murmur3_test::murmur3_128_vectors;
using murmur3_test::murmur3_32_vectors;

constexpr std::array<std::byte, murmur3_test::prefix_count - 1> message = [] {
  std::array<std::byte, murmur3_test::prefix_count - 1> result{};
  murmur3_test::fill_message(result);
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
  murmur3_state<Width> state{.seed = seed};
  for(; data.size() > chunk_size; data = data.subspan(chunk_size)) {
    state = murmur3_update(state, data.first(chunk_size));
  }
  return murmur3_finalize(murmur3_update(state, data));
}

using namespace std::literals;

static_assert(murmur3_compute<32>(""sv) == 0);
static_assert(murmur3_compute<32>(""sv, 1) == 0x514E28B7U);
static_assert(murmur3_compute<32>("Hello, world!"sv, 1234) == 0xFAF6CDB3U);
static_assert(murmur3_compute<128>(""sv) == hash128{0, 0});
static_assert(murmur3_compute<128>("Hello, world!"sv, 1234) == hash128{0x61130E64AA0AC6FEU, 0x51F9046D087E1B56U});
static_assert(murmur3_finalize(murmur3_state<32>{.seed = 1}) == 0x514E28B7U);
static_assert(murmur3_compute<32>(std::span(message), murmur3_32_vectors[2].seed) == murmur3_32_vectors[2].prefixes[300]);
static_assert(murmur3_compute<128>(std::span(message).first(33), murmur3_128_vectors[2].seed) == murmur3_128_vectors[2].prefixes[33]);
static_assert(chunked<128>(message, 7, murmur3_128_vectors[1].seed) == murmur3_128_vectors[1].prefixes[300]);
static_assert(chunked<32>(message, 3, murmur3_32_vectors[1].seed) == murmur3_32_vectors[1].prefixes[300]);
// Every tail length around a few blocks; longer prefixes exceed the constant-evaluation step limit of Clang.
static_assert(prefix_mismatches<32>(70) == 0);
static_assert(prefix_mismatches<128>(70) == 0);

template <class Width> class murmur3 : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 128>>;
// Names the test instances x86_32 and x64_128.
struct width_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Width> static std::string GetName(int /*index*/) { return Width::value == 32 ? "x86_32" : "x64_128"; }
};

TYPED_TEST_SUITE(murmur3, widths, width_names);

TYPED_TEST(murmur3, reference_prefixes) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t size = 0; size < murmur3_test::prefix_count; ++size) {
      auto const data = std::span(message).first(size);
      ASSERT_EQ(murmur3_compute<width>(data, set.seed), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
      ASSERT_EQ(murmur3_finalize(murmur3_update(murmur3_state<width>{.seed = set.seed}, data)), set.prefixes[size])
          << "seed " << set.seed << ", size " << size;
    }
  }
}

TYPED_TEST(murmur3, unaligned_starts) {
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
      murmur3_state<width> state = murmur3_update(murmur3_state<width>{.seed = set.seed}, data.first(first));
      state = murmur3_update(state, data.subspan(first, second - first));
      ASSERT_EQ(murmur3_finalize(state), set.prefixes[second]) << "split at " << first << " and " << second;
      ASSERT_EQ(murmur3_finalize(murmur3_update(state, data.subspan(second))), set.prefixes[size]) << "split at " << first << " and " << second;
    }
  }
}

// Pieces of every size up to past two 32-byte spans, so that splits fall at every offset around the 4- and 16-byte block boundaries.
TYPED_TEST(murmur3, chunks_across_blocks) {
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
  EXPECT_EQ(murmur3_finalize(murmur3_update(murmur3_state<width>{.seed = set.seed}, text)), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(murmur3_compute<width>(std::list<std::byte>(message.begin(), message.end()), set.seed), set.prefixes[300]);
}

TYPED_TEST(murmur3, long_message) {
  constexpr unsigned width = TypeParam::value;
  std::vector<std::byte> data(murmur3_test::long_message_size);
  murmur3_test::fill_message(data);
  for(auto const &set : vectors<width>()) {
    EXPECT_EQ(murmur3_compute<width>(data, set.seed), set.long_message) << "seed " << set.seed;
    EXPECT_EQ(chunked<width>(data, 4099, set.seed), set.long_message) << "seed " << set.seed;
  }
}

} // namespace
