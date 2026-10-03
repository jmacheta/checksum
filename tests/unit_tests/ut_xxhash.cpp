// XXH32 and XXH64: reference values, splits, alignments and ranges.

#include <checksum/xxhash.hpp>
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
using xxhash_test::xxhash32_vectors;
using xxhash_test::xxhash64_vectors;

constexpr std::array<std::byte, xxhash_test::prefix_count - 1> message = [] {
  std::array<std::byte, xxhash_test::prefix_count - 1> result{};
  xxhash_test::fill_message(result);
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
  xxhash_test::fill_message(data);
  for(auto const &set : vectors<width>()) {
    EXPECT_EQ(xxhash_compute<width>(data, set.seed), set.long_message) << "seed " << set.seed;
    EXPECT_EQ(chunked<width>(data, 4099, set.seed), set.long_message) << "seed " << set.seed;
  }
}

} // namespace
