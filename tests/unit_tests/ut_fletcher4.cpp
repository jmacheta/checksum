// ZFS fletcher4: OpenZFS reference values, an independent model, splits, ranges, resume and edge values.

#include <checksum/engine.hpp>
#include <checksum/fletcher4.hpp>

#include <fletcher4_reference_vectors.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// Every prefix of this size stays within the default step limit of Clang's constant evaluator.
constexpr std::size_t constant_message_size = 200;

constexpr auto constant_message = random_array<constant_message_size>();

static_assert(fletcher4_compute("abcdefgh"sv) == fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
static_assert(fletcher4_compute(std::span<std::byte const>{}) == fletcher4_value{});
static_assert(fletcher4_compute(std::span(constant_message).first(4)) == fletcher4_test::prefixes[1]);
static_assert(fletcher4_compute(std::span(constant_message).first(64)) == fletcher4_test::prefixes[16]);
static_assert(fletcher4_compute(std::span(constant_message).first(200)) == fletcher4_test::prefixes[50]);

// Independent model: whole words read byte by byte with zero padding, written out as in the definition.
fletcher4_value reference(std::span<std::byte const> data) {
  // NOLINTBEGIN(readability-identifier-length): the names of the definition.
  std::uint64_t a = 0;
  std::uint64_t b = 0;
  std::uint64_t c = 0;
  std::uint64_t d = 0;
  // NOLINTEND(readability-identifier-length)
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

TEST(fletcher4, reference_values) {
  std::vector<std::byte> data(fletcher4_test::long_message_size);
  fill_random(data);
  auto const message = std::span<std::byte const>(data);
  for(std::size_t words = 0; words < fletcher4_test::prefix_count; ++words) {
    ASSERT_EQ(fletcher4_compute(message.first(4 * words)), fletcher4_test::prefixes[words]) << "words " << words;
    ASSERT_EQ(reference(message.first(4 * words)), fletcher4_test::prefixes[words]) << "words " << words;
  }
}

// The OpenZFS value of a message of 1 MiB, one-shot and in 4099-byte pieces.
TEST(fletcher4, long_message) {
  std::vector<std::byte> data(fletcher4_test::long_message_size);
  fill_random(data);
  EXPECT_EQ(fletcher4_compute(data), fletcher4_test::long_message);
  EXPECT_EQ(reference(data), fletcher4_test::long_message);
  EXPECT_EQ(engine_checks::chunked(fletcher4_engine{}, data, 4099), fletcher4_test::long_message);
}

// All lengths, including unfinished words, at every alignment of a word.
TEST(fletcher4, every_size_and_offset) {
  auto const data = random_bytes(1100 + 16, 4);
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 0; size <= 1100; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      ASSERT_EQ(fletcher4_compute(message), reference(message)) << "offset " << offset << ", size " << size;
    }
  }
}

// sum3 and sum4 wrap around 2^64; the other sizes end with an unfinished word.
TEST(fletcher4, long_all_ones) {
  std::vector<std::byte> const data(fletcher4_test::all_ones_size, std::byte{0xFF});
  EXPECT_EQ(fletcher4_compute(data), fletcher4_test::all_ones);
  EXPECT_EQ(reference(data), fletcher4_test::all_ones);
  for(std::size_t const size : {fletcher4_test::all_ones_size + 1, fletcher4_test::all_ones_size + 3}) {
    std::vector<std::byte> const longer(size, std::byte{0xFF});
    ASSERT_EQ(fletcher4_compute(longer), reference(longer)) << "size " << size;
  }
}

TEST(fletcher4, split_anywhere) { engine_checks::check_split_anywhere(fletcher4_engine{}, random_bytes(80, 3)); }

// An unfinished word entering long chunks, and pieces of every size up to 70 bytes.
TEST(fletcher4, split_into_chunks) {
  auto const data = random_bytes(5000, 5);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference(message);
  for(std::size_t const first : {1U, 2U, 3U, 5U, 63U, 255U, 257U, 1023U, 1501U, 4093U}) {
    fletcher4_state const state =
        fletcher4_engine{}.update(fletcher4_engine{}.update(fletcher4_state{}, message.first(first)), message.subspan(first));
    ASSERT_EQ(fletcher4_engine{}.finalize(state), whole) << "split at " << first;
  }
  for(std::size_t chunk_size = 1; chunk_size <= 70; ++chunk_size) {
    ASSERT_EQ(engine_checks::chunked(fletcher4_engine{}, message.first(300), chunk_size), reference(message.first(300))) << "chunk " << chunk_size;
  }
}

// Sizes around the lane thresholds of every target (128 to 384 bytes) and around the 16-byte groups, at every alignment, continuing a
// state whose sums wrap: the combination of the lanes with the earlier sums.
TEST(fletcher4, lanes_continue_state) {
  auto const data = random_bytes(600 + 16, 7);
  constexpr fletcher4_state start{
      .sum1 = 0xFFFF'FFFF'FFFF'FFF0, .sum2 = 0x8765'4321'0FED'CBA9, .sum3 = ~std::uint64_t{0}, .sum4 = 0x0123'4567'89AB'CDEF};
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 100; size <= 600; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      fletcher4_state expected = start;
      for(std::size_t start_byte = 0; start_byte + 4 <= size; start_byte += 4) {
        expected.sum1 += reference(message.subspan(start_byte, 4))[0];
        expected.sum2 += expected.sum1;
        expected.sum3 += expected.sum2;
        expected.sum4 += expected.sum3;
      }
      expected.sum1 += reference(message.subspan(size - (size % 4)))[0];
      expected.word_offset = static_cast<std::uint8_t>(size % 4);
      ASSERT_EQ(fletcher4_engine{}.update(start, message), expected) << "offset " << offset << ", size " << size;
    }
  }
}

// Every prefix computed at compile time, plus splits inside a word.
TEST(fletcher4, constant_evaluation) {
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
      result[first] = fletcher4_engine{}.update(fletcher4_engine{}.update(fletcher4_state{}, std::span(constant_message).first(first + 5)),
                                                std::span(constant_message).subspan(first + 5));
    }
    return result;
  }();
  auto const message = std::span<std::byte const>(constant_message);
  for(std::size_t size = 0; size <= constant_message_size; ++size) {
    ASSERT_EQ(prefixes[size], fletcher4_compute(message.first(size))) << "size " << size;
  }
  for(std::size_t first = 0; first < splits.size(); ++first) {
    EXPECT_EQ(splits[first],
              fletcher4_engine{}.update(fletcher4_engine{}.update(fletcher4_state{}, message.first(first + 5)), message.subspan(first + 5)));
  }
}

static_assert(resumable_engine<fletcher4_engine<>> && resumable_engine<fletcher4_dispatch_engine>);
static_assert(fletcher4_engine{}.resume({1, 2, 3, 4}) == fletcher4_state{.sum1 = 1, .sum2 = 2, .sum3 = 3, .sum4 = 4});

// A stored checksum continues the message after any whole number of words, at compile time and at run time.
TEST(fletcher4, resume_from_checksum) {
  static constexpr fletcher4_engine fletcher4;
  static constexpr auto resumed = [] {
    std::array<fletcher4_value, (constant_message_size / 4) + 1> result{};
    for(std::size_t word = 0; word < result.size(); ++word) {
      auto const message = std::span(constant_message);
      result[word] = fletcher4.finalize(fletcher4.update(fletcher4.resume(fletcher4_compute(message.first(4 * word))), message.subspan(4 * word)));
    }
    return result;
  }();
  for(fletcher4_value const &value : resumed) {
    ASSERT_EQ(value, fletcher4_compute(constant_message));
  }

  auto const data = random_bytes(1101, 9);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference(message);
  for(std::size_t first = 0; first <= data.size(); first += 4) {
    ASSERT_EQ(fletcher4.finalize(fletcher4.update(fletcher4.resume(fletcher4_compute(message.first(first))), message.subspan(first))), whole)
        << "split at " << first;
  }
  std::list<std::byte> const tail(data.begin() + 100, data.end());
  EXPECT_EQ(fletcher4.finalize(fletcher4.update(fletcher4.resume(fletcher4_compute(message.first(100))), tail)), whole);
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
  EXPECT_EQ(fletcher4_engine{}.update(fletcher4_state{}, std::list<std::byte>(data.begin(), data.end())),
            fletcher4_engine{}.update(fletcher4_state{}, data));
}

// The word offset is taken modulo 4 from the largest sums, at compile time and at run time.
TEST(fletcher4, edge_values) {
  auto const data = random_bytes(37, 6);
  auto const message = std::span<std::byte const>(data);
  constexpr std::uint64_t largest = ~std::uint64_t{0};
  for(unsigned offset = 0; offset < 256; ++offset) {
    fletcher4_state const state{.sum1 = largest, .sum2 = largest, .sum3 = largest, .sum4 = largest, .word_offset = static_cast<std::uint8_t>(offset)};
    fletcher4_state masked = state;
    masked.word_offset = static_cast<std::uint8_t>(offset % 4);
    auto const result = fletcher4_engine{}.update(state, message);
    ASSERT_EQ(result, fletcher4_engine{}.update(masked, message)) << "offset " << offset;
    ASSERT_EQ(fletcher4_engine{}.finalize(state), fletcher4_engine{}.finalize(masked)) << "offset " << offset;
    ASSERT_LT(result.word_offset, 4U);
  }
  constexpr fletcher4_state maximum{.word_offset = 0xFF};
  static_assert(fletcher4_engine{}.update(maximum, std::span<std::byte const>{}) ==
                fletcher4_engine{}.update(fletcher4_state{.word_offset = 3}, std::span<std::byte const>{}));
}

} // namespace
