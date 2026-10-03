// XXH3-64 and XXH3-128: reference values, splits, alignments, ranges and the portable multiplication.

#include <checksum/xxh3.hpp>
#include <xxh3_reference_vectors.hpp>
#include <xxhash_reference_vectors.hpp>

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
using xxhash_test::block_sizes;
using xxhash_test::xxh3_128_vectors;
using xxhash_test::xxh3_64_vectors;

constexpr std::size_t prefix_limit = xxhash_test::prefix_count - 1;

// The message of the reference vectors, as long as the longest block size.
constexpr std::array<std::byte, block_sizes.back()> message = [] {
  std::array<std::byte, block_sizes.back()> result{};
  xxhash_test::fill_message(result);
  return result;
}();

template <unsigned Width> constexpr auto const &vectors() {
  if constexpr(Width == 64) {
    return xxh3_64_vectors;
  } else {
    return xxh3_128_vectors;
  }
}

// Number of prefixes of message up to size_limit bytes whose hash differs from the reference, for every seed.
template <unsigned Width> constexpr std::size_t prefix_mismatches(std::size_t size_limit) {
  std::size_t mismatches = 0;
  for(auto const &set : vectors<Width>()) {
    for(std::size_t size = 0; size <= size_limit; ++size) {
      mismatches += xxh3_compute<Width>(std::span(message).first(size), set.seed) != set.prefixes[size] ? 1U : 0U;
    }
  }
  return mismatches;
}

// The hash of data, folded in pieces of chunk_size bytes.
template <unsigned Width> constexpr auto chunked(std::span<std::byte const> data, std::size_t chunk_size, std::uint64_t seed) {
  xxh3_state<Width> state{.seed = seed};
  for(; data.size() > chunk_size; data = data.subspan(chunk_size)) {
    state = xxh3_update(state, data.first(chunk_size));
  }
  return xxh3_finalize(xxh3_update(state, data));
}

using namespace std::literals;

static_assert(xxh3_compute<64>(""sv) == 0x2D06800538D394C2U);
static_assert(xxh3_compute<64>("abc"sv) == 0x78AF5F94892F3950U);
static_assert(xxh3_compute<128>("abc"sv) == xxh3_hash128{.low = 0x78AF5F94892F3950U, .high = 0x06B05AB6733A6185U});
static_assert(xxh3_finalize(xxh3_state<128>{}) == xxh3_128_vectors[0].prefixes[0]);
// Every length class up to the first medium one.
static_assert(prefix_mismatches<64>(20) == 0);
static_assert(prefix_mismatches<128>(20) == 0);
static_assert(xxh3_compute<64>(std::span(message).first(129), xxh3_64_vectors[2].seed) == xxh3_64_vectors[2].prefixes[129]);
static_assert(xxh3_compute<128>(std::span(message).first(240), xxh3_128_vectors[2].seed) == xxh3_128_vectors[2].prefixes[240]);
static_assert(xxh3_compute<128>(std::span(message).first(241), xxh3_128_vectors[1].seed) == xxh3_128_vectors[1].prefixes[241]);
static_assert(xxh3_compute<64>(std::span(message).first(block_sizes[2]), xxh3_64_vectors[2].seed) == xxh3_64_vectors[2].blocks[2]);
static_assert(chunked<128>(std::span(message).first(prefix_limit), 7, xxh3_128_vectors[2].seed) == xxh3_128_vectors[2].prefixes[prefix_limit]);
static_assert(chunked<64>(std::span(message).first(block_sizes[2]), 100, xxh3_64_vectors[1].seed) == xxh3_64_vectors[1].blocks[2]);
static_assert(xxh3_detail::multiply_portable(~std::uint64_t{0}, ~std::uint64_t{0}) == xxh3_hash128{.low = 1, .high = ~std::uint64_t{1}});
static_assert(xxh3_detail::multiply_portable(0x9E3779B185EBCA87U, 0xC2B2AE3D27D4EB4FU) ==
              xxh3_hash128{.low = 0xDEF35B010F796CA9U, .high = 0x7854787AA57880A8U});

template <class Width> class xxh3 : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 64>, std::integral_constant<unsigned, 128>>;
// Names the test instances xxh3_64 and xxh3_128.
struct width_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Width> static std::string GetName(int /*index*/) { return "xxh3_" + std::to_string(Width::value); }
};

TYPED_TEST_SUITE(xxh3, widths, width_names);

TYPED_TEST(xxh3, reference_values) {
  constexpr unsigned width = TypeParam::value;
  for(auto const &set : vectors<width>()) {
    for(std::size_t size = 0; size <= prefix_limit; ++size) {
      auto const data = std::span(message).first(size);
      ASSERT_EQ(xxh3_compute<width>(data, set.seed), set.prefixes[size]) << "seed " << set.seed << ", size " << size;
      ASSERT_EQ(xxh3_finalize(xxh3_update(xxh3_state<width>{.seed = set.seed}, data)), set.prefixes[size])
          << "seed " << set.seed << ", size " << size;
    }
    for(std::size_t index = 0; index < block_sizes.size(); ++index) {
      auto const data = std::span(message).first(block_sizes[index]);
      ASSERT_EQ(xxh3_compute<width>(data, set.seed), set.blocks[index]) << "seed " << set.seed << ", size " << block_sizes[index];
      ASSERT_EQ(xxh3_finalize(xxh3_update(xxh3_state<width>{.seed = set.seed}, data)), set.blocks[index])
          << "seed " << set.seed << ", size " << block_sizes[index];
    }
  }
}

TYPED_TEST(xxh3, unaligned_starts) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[2];
  for(std::size_t offset = 0; offset < 16; ++offset) {
    std::vector<std::byte> buffer(offset + message.size());
    std::ranges::copy(message, buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    for(std::size_t size = 0; size <= prefix_limit; ++size) {
      ASSERT_EQ(xxh3_compute<width>(std::span(buffer).subspan(offset, size), set.seed), set.prefixes[size])
          << "offset " << offset << ", size " << size;
    }
    for(std::size_t index = 0; index < block_sizes.size(); ++index) {
      ASSERT_EQ(xxh3_compute<width>(std::span(buffer).subspan(offset, block_sizes[index]), set.seed), set.blocks[index])
          << "offset " << offset << ", size " << block_sizes[index];
    }
  }
}

TYPED_TEST(xxh3, split_anywhere) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[2];
  auto const data = std::span<std::byte const>(message).first(prefix_limit);
  for(std::size_t first = 0; first <= prefix_limit; ++first) {
    for(std::size_t second = first; second <= prefix_limit; ++second) {
      xxh3_state<width> state = xxh3_update(xxh3_state<width>{.seed = set.seed}, data.first(first));
      state = xxh3_update(state, data.subspan(first, second - first));
      ASSERT_EQ(xxh3_finalize(state), set.prefixes[second]) << "split at " << first << " and " << second;
      ASSERT_EQ(xxh3_finalize(xxh3_update(state, data.subspan(second))), set.prefixes[prefix_limit]) << "split at " << first << " and " << second;
    }
  }
}

// Pieces of every size up to past one 64-byte stripe and around the 256-byte buffer and 1024-byte blocks, over messages that end around
// the buffer and the blocks.
TYPED_TEST(xxh3, chunks_across_stripes_and_blocks) {
  constexpr unsigned width = TypeParam::value;
  std::vector<std::size_t> chunk_sizes{127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024, 1025, 2047, 2048, 2049};
  for(std::size_t chunk_size = 1; chunk_size <= 70; ++chunk_size) {
    chunk_sizes.push_back(chunk_size);
  }
  for(auto const &set : vectors<width>()) {
    for(std::size_t const chunk_size : chunk_sizes) {
      for(std::size_t const size : {240U, 241U, 255U, 256U, 257U, 300U}) {
        ASSERT_EQ(chunked<width>(std::span(message).first(size), chunk_size, set.seed), set.prefixes[size])
            << "chunk " << chunk_size << ", size " << size;
      }
      for(std::size_t index = 0; index < block_sizes.size(); ++index) {
        ASSERT_EQ(chunked<width>(std::span(message).first(block_sizes[index]), chunk_size, set.seed), set.blocks[index])
            << "chunk " << chunk_size << ", size " << block_sizes[index];
      }
    }
  }
}

// The longest block message split once around stripes, the 256-byte buffer and the 1024-byte blocks, from every start offset of a
// 16-byte vector.
TYPED_TEST(xxh3, splits_across_blocks_from_any_offset) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[1];
  std::vector<std::size_t> splits;
  for(std::size_t const boundary : {64U, 256U, 960U, 1024U, 1088U, 2048U, 3072U, 4032U, 4096U}) {
    for(std::size_t const split : {boundary - 1, boundary, boundary + 1}) {
      splits.push_back(split);
    }
  }
  for(std::size_t offset = 0; offset < 16; ++offset) {
    std::vector<std::byte> buffer(offset + message.size());
    std::ranges::copy(message, buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    auto const data = std::span<std::byte const>(buffer).subspan(offset);
    for(std::size_t const split : splits) {
      xxh3_state<width> const state = xxh3_update(xxh3_state<width>{.seed = set.seed}, data.first(split));
      ASSERT_EQ(xxh3_finalize(xxh3_update(state, data.subspan(split))), set.blocks.back()) << "offset " << offset << ", split " << split;
    }
  }
}

TYPED_TEST(xxh3, byte_ranges) {
  constexpr unsigned width = TypeParam::value;
  auto const &set = vectors<width>()[1];
  constexpr std::string_view text = "123456789 123456789 123456789 123456789";
  auto const expected = xxh3_compute<width>(std::as_bytes(std::span(text)), set.seed);
  EXPECT_EQ(xxh3_compute<width>(text, set.seed), expected);
  EXPECT_EQ(xxh3_compute<width>(std::list<char>(text.begin(), text.end()), set.seed), expected);
  EXPECT_EQ(xxh3_compute<width>(std::vector<unsigned char>(text.begin(), text.end()), set.seed), expected);
  EXPECT_EQ(xxh3_finalize(xxh3_update(xxh3_state<width>{.seed = set.seed}, text)), expected);
  // Non-contiguous ranges in 64-byte chunks, over the buffer and over blocks.
  EXPECT_EQ(xxh3_compute<width>(std::list<std::byte>(message.begin(), message.begin() + prefix_limit), set.seed), set.prefixes[prefix_limit]);
  EXPECT_EQ(xxh3_compute<width>(std::list<std::byte>(message.begin(), message.end()), set.seed), set.blocks.back());
}

TYPED_TEST(xxh3, long_message) {
  constexpr unsigned width = TypeParam::value;
  std::vector<std::byte> data(xxhash_test::long_message_size);
  xxhash_test::fill_message(data);
  for(auto const &set : vectors<width>()) {
    EXPECT_EQ(xxh3_compute<width>(data, set.seed), set.long_message) << "seed " << set.seed;
    EXPECT_EQ(chunked<width>(data, 4099, set.seed), set.long_message) << "seed " << set.seed;
  }
}

// The run-time multiplication, native where the compiler has one, against the portable one.
TEST(xxh3_multiply, matches_portable) {
  std::uint64_t left = 0x0123456789ABCDEFU;
  std::uint64_t right = ~std::uint64_t{0};
  for(int step = 0; step < 1000; ++step) {
    ASSERT_EQ(xxh3_detail::multiply(left, right), xxh3_detail::multiply_portable(left, right)) << left << " * " << right;
    left = (left * 0x9E3779B97F4A7C15U) + 1;
    right ^= right << 7U;
    right ^= right >> 9U;
  }
}

} // namespace
