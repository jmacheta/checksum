// ZFS fletcher4: OpenZFS reference values, an independent model, splits, ranges and wraparound.

#include <checksum/fletcher4.hpp>

#include <fletcher4_reference_vectors.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <random>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// Every prefix of this size stays within the default step limit of Clang's constant evaluator.
constexpr std::size_t constant_message_size = 200;

constexpr std::array<std::byte, constant_message_size> constant_message = [] {
  std::array<std::byte, constant_message_size> result{};
  fletcher4_test::fill_message(result);
  return result;
}();

static_assert(fletcher4_compute("abcdefgh"sv) == fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
static_assert(fletcher4_compute(std::span<std::byte const>{}) == fletcher4_value{});
static_assert(fletcher4_compute(std::span(constant_message).first(4)) == fletcher4_test::prefixes[1]);
static_assert(fletcher4_compute(std::span(constant_message).first(64)) == fletcher4_test::prefixes[16]);
static_assert(fletcher4_compute(std::span(constant_message).first(200)) == fletcher4_test::prefixes[50]);

// Independent model: whole words read byte by byte with zero padding, written out as in the definition.
fletcher4_value reference(std::span<std::byte const> data) {
  std::uint64_t a = 0;
  std::uint64_t b = 0;
  std::uint64_t c = 0;
  std::uint64_t d = 0;
  for(std::size_t start = 0; start < data.size(); start += 4) {
    std::uint64_t word = 0;
    for(std::size_t index = 0; index < 4 && start + index < data.size(); ++index) {
      word |= std::to_integer<std::uint64_t>(data[start + index]) << (8 * index);
    }
    a += word;
    b += a;
    c += b;
    d += c;
  }
  return {a, b, c, d};
}

std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed) {
  std::mt19937 generator(seed);
  std::vector<std::byte> data(size);
  for(auto &byte : data) {
    byte = static_cast<std::byte>(generator());
  }
  return data;
}

TEST(fletcher4, reference_vectors) {
  std::vector<std::byte> data(fletcher4_test::long_message_size);
  fletcher4_test::fill_message(data);
  auto const message = std::span<std::byte const>(data);
  for(std::size_t words = 0; words < fletcher4_test::prefix_count; ++words) {
    ASSERT_EQ(fletcher4_compute(message.first(4 * words)), fletcher4_test::prefixes[words]) << "words " << words;
    ASSERT_EQ(reference(message.first(4 * words)), fletcher4_test::prefixes[words]) << "words " << words;
  }
  EXPECT_EQ(fletcher4_compute(message), fletcher4_test::long_message);
  EXPECT_EQ(reference(message), fletcher4_test::long_message);
  fletcher4_state state{};
  for(std::size_t start = 0; start < message.size(); start += 4099) {
    state = fletcher4_update(state, message.subspan(start, std::min<std::size_t>(4099, message.size() - start)));
  }
  EXPECT_EQ(fletcher4_finalize(state), fletcher4_test::long_message);
}

// All lengths, including unfinished words, at every alignment of a word.
TEST(fletcher4, matches_model) {
  auto const data = random_bytes(1100 + 16, 4);
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 0; size <= 1100; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      ASSERT_EQ(fletcher4_compute(message), reference(message)) << "offset " << offset << ", size " << size;
    }
  }
}

// sum3 and sum4 wrap around 2^64; the other sizes end with an unfinished word.
TEST(fletcher4, all_ones) {
  std::vector<std::byte> const data(fletcher4_test::all_ones_size, std::byte{0xFF});
  EXPECT_EQ(fletcher4_compute(data), fletcher4_test::all_ones);
  EXPECT_EQ(reference(data), fletcher4_test::all_ones);
  for(std::size_t const size : {fletcher4_test::all_ones_size + 1, fletcher4_test::all_ones_size + 3}) {
    std::vector<std::byte> const longer(size, std::byte{0xFF});
    ASSERT_EQ(fletcher4_compute(longer), reference(longer)) << "size " << size;
  }
}

TEST(fletcher4, split_anywhere) {
  auto const data = random_bytes(80, 3);
  auto const message = std::span<std::byte const>(data);
  fletcher4_state const whole = fletcher4_update(fletcher4_state{}, message);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    for(std::size_t second = first; second <= data.size(); ++second) {
      fletcher4_state state = fletcher4_update(fletcher4_state{}, message.first(first));
      state = fletcher4_update(state, message.subspan(first, second - first));
      state = fletcher4_update(state, message.subspan(second));
      ASSERT_EQ(state, whole) << "split at " << first << " and " << second;
    }
  }
}

// An unfinished word entering long chunks.
TEST(fletcher4, split_before_long_chunks) {
  auto const data = random_bytes(5000, 5);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference(message);
  for(std::size_t const first : {1U, 2U, 3U, 5U, 63U, 255U, 257U, 1023U, 1501U, 4093U}) {
    fletcher4_state const state = fletcher4_update(fletcher4_update(fletcher4_state{}, message.first(first)), message.subspan(first));
    ASSERT_EQ(fletcher4_finalize(state), whole) << "split at " << first;
  }
}

// Every prefix computed at compile time, plus splits inside a word.
TEST(fletcher4, constant_evaluation_matches_run_time) {
  static constexpr auto prefixes = [] {
    std::array<fletcher4_value, constant_message_size + 1> result{};
    for(std::size_t size = 0; size <= constant_message_size; ++size) {
      result[size] = fletcher4_compute(std::span(constant_message).first(size));
    }
    return result;
  }();
  static constexpr auto splits = [] {
    std::array<fletcher4_state, 4> result{};
    for(std::size_t first = 0; first < result.size(); ++first) {
      result[first] = fletcher4_update(fletcher4_update(fletcher4_state{}, std::span(constant_message).first(first + 5)),
                                       std::span(constant_message).subspan(first + 5));
    }
    return result;
  }();
  auto const message = std::span<std::byte const>(constant_message);
  for(std::size_t size = 0; size <= constant_message_size; ++size) {
    ASSERT_EQ(prefixes[size], fletcher4_compute(message.first(size))) << "size " << size;
  }
  for(std::size_t first = 0; first < splits.size(); ++first) {
    EXPECT_EQ(splits[first], fletcher4_update(fletcher4_update(fletcher4_state{}, message.first(first + 5)), message.subspan(first + 5)));
  }
}

TEST(fletcher4, byte_ranges) {
  constexpr std::string_view text = "123456789";
  auto const expected = fletcher4_compute(std::as_bytes(std::span(text)));
  EXPECT_EQ(fletcher4_compute(text), expected);
  EXPECT_EQ(fletcher4_compute(std::list<char>(text.begin(), text.end())), expected);
  EXPECT_EQ(fletcher4_compute(std::vector<unsigned char>(text.begin(), text.end())), expected);
  // Longer than one 64-byte chunk of a non-contiguous range, chunks ending inside a word.
  auto const data = random_bytes(203, 4);
  EXPECT_EQ(fletcher4_compute(std::list<std::byte>(data.begin(), data.end())), reference(data));
  EXPECT_EQ(fletcher4_update(fletcher4_state{}, std::list<std::byte>(data.begin(), data.end())), fletcher4_update(fletcher4_state{}, data));
}

// The word offset is taken modulo 4, at compile time and at run time.
TEST(fletcher4, word_offset_modulo_4) {
  auto const data = random_bytes(37, 6);
  auto const message = std::span<std::byte const>(data);
  constexpr std::uint64_t largest = ~std::uint64_t{0};
  for(unsigned offset = 0; offset < 256; ++offset) {
    fletcher4_state const state{.sum1 = largest, .sum2 = largest, .sum3 = largest, .sum4 = largest, .word_offset = static_cast<std::uint8_t>(offset)};
    fletcher4_state masked = state;
    masked.word_offset = static_cast<std::uint8_t>(offset % 4);
    auto const result = fletcher4_update(state, message);
    ASSERT_EQ(result, fletcher4_update(masked, message)) << "offset " << offset;
    ASSERT_EQ(fletcher4_finalize(state), fletcher4_finalize(masked)) << "offset " << offset;
    ASSERT_LT(result.word_offset, 4U);
  }
  constexpr fletcher4_state maximum{.word_offset = 0xFF};
  static_assert(fletcher4_update(maximum, std::span<std::byte const>{}) ==
                fletcher4_update(fletcher4_state{.word_offset = 3}, std::span<std::byte const>{}));
}

} // namespace
