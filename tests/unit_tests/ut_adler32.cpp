// Adler-32 checksum: published examples, an independent model, splits, ranges and edge states.

#include <checksum/adler32.hpp>

#include <gtest/gtest.h>

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

// Examples of RFC 1950 and the Wikipedia article "Adler-32", checked against the reference model below and Python's zlib.adler32.
static_assert(adler32_compute(std::span<std::byte const>{}) == 0x00000001);
static_assert(adler32_compute("a"sv) == 0x00620062);
static_assert(adler32_compute("abc"sv) == 0x024D0127);
static_assert(adler32_compute("Wikipedia"sv) == 0x11E60398);
static_assert(adler32_compute("123456789"sv) == 0x091E01DE);

// Independent model: each sum reduced with % after every byte, starting from the given sums.
std::uint32_t reference(std::span<std::byte const> data, std::uint32_t sum1 = 1, std::uint32_t sum2 = 0) {
  constexpr std::uint32_t modulus = 65521;
  sum1 %= modulus;
  sum2 %= modulus;
  for(std::byte const byte : data) {
    sum1 = (sum1 + std::to_integer<std::uint32_t>(byte)) % modulus;
    sum2 = (sum2 + sum1) % modulus;
  }
  return (sum2 << 16U) | sum1;
}

std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed) {
  std::mt19937 generator(seed);
  std::vector<std::byte> data(size);
  for(auto &byte : data) {
    byte = static_cast<std::byte>(generator());
  }
  return data;
}

TEST(adler32, published_examples) {
  for(auto const text : {""sv, "a"sv, "abc"sv, "Wikipedia"sv, "123456789"sv}) {
    auto const message = std::as_bytes(std::span(text));
    EXPECT_EQ(adler32_compute(message), reference(message)) << text;
  }
  EXPECT_EQ(adler32_compute(std::span<std::byte const>{}), 0x00000001U);
  EXPECT_EQ(adler32_compute("a"sv), 0x00620062U);
  EXPECT_EQ(adler32_compute("abc"sv), 0x024D0127U);
  EXPECT_EQ(adler32_compute("Wikipedia"sv), 0x11E60398U);
  EXPECT_EQ(adler32_compute("123456789"sv), 0x091E01DEU);
}

// All lengths past the unrolled loop and its tail, at every alignment of a word.
TEST(adler32, matches_model) {
  auto const data = random_bytes(1100 + 16, 32);
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 0; size <= 1100; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      ASSERT_EQ(adler32_compute(message), reference(message)) << "offset " << offset << ", size " << size;
    }
  }
}

// The largest bytes from the largest canonical sums, over many deferred reductions of 5'552 bytes on 32-bit targets.
TEST(adler32, all_ones) {
  adler32_state const largest{.sum1 = 65520, .sum2 = 65520};
  for(std::size_t const size : {(std::size_t{6} << 20), (std::size_t{6} << 20) + 1, (std::size_t{6} << 20) + 7, std::size_t{5552 * 1000}}) {
    std::vector<std::byte> const data(size, std::byte{0xFF});
    ASSERT_EQ(adler32_compute(data), reference(data)) << "size " << size;
    ASSERT_EQ(adler32_finalize(adler32_update(largest, data)), reference(data, 65520, 65520)) << "size " << size;
  }
}

TEST(adler32, split_anywhere) {
  auto const data = random_bytes(80, 3);
  auto const message = std::span<std::byte const>(data);
  adler32_state const whole = adler32_update(adler32_state{}, message);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    for(std::size_t second = first; second <= data.size(); ++second) {
      adler32_state state = adler32_update(adler32_state{}, message.first(first));
      state = adler32_update(state, message.subspan(first, second - first));
      state = adler32_update(state, message.subspan(second));
      ASSERT_EQ(state, whole) << "split at " << first << " and " << second;
    }
  }
}

// Short and odd prefixes entering chunks long enough for the unrolled loop and several deferred reductions.
TEST(adler32, split_before_long_chunks) {
  auto const data = random_bytes(20000, 5);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference(message);
  for(std::size_t const first : {1U, 2U, 3U, 5U, 7U, 63U, 255U, 257U, 1023U, 1501U, 4093U, 5551U, 5553U}) {
    adler32_state const state = adler32_update(adler32_update(adler32_state{}, message.first(first)), message.subspan(first));
    ASSERT_EQ(adler32_finalize(state), whole) << "split at " << first;
  }
}

// Every prefix of this size stays within the default step limit of Clang's constant evaluator.
constexpr std::size_t constant_message_size = 200;

// Pseudo-random bytes the constant evaluator can produce (xorshift32).
constexpr std::array<std::byte, constant_message_size> constant_message = [] {
  std::array<std::byte, constant_message_size> result{};
  std::uint32_t state = 0x12345678U;
  for(auto &byte : result) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    byte = static_cast<std::byte>(state);
  }
  return result;
}();

// Every prefix computed at compile time, plus a few splits.
TEST(adler32, constant_evaluation_matches_run_time) {
  static constexpr auto prefixes = [] {
    std::array<std::uint32_t, constant_message_size + 1> result{};
    for(std::size_t size = 0; size <= constant_message_size; ++size) {
      result[size] = adler32_compute(std::span(constant_message).first(size));
    }
    return result;
  }();
  static constexpr auto splits = [] {
    std::array<adler32_state, 4> result{};
    for(std::size_t first = 0; first < result.size(); ++first) {
      result[first] = adler32_update(adler32_update(adler32_state{}, std::span(constant_message).first(first + 5)),
                                     std::span(constant_message).subspan(first + 5));
    }
    return result;
  }();
  auto const message = std::span<std::byte const>(constant_message);
  for(std::size_t size = 0; size <= constant_message_size; ++size) {
    ASSERT_EQ(prefixes[size], adler32_compute(message.first(size))) << "size " << size;
  }
  for(std::size_t first = 0; first < splits.size(); ++first) {
    EXPECT_EQ(splits[first], adler32_update(adler32_update(adler32_state{}, message.first(first + 5)), message.subspan(first + 5)));
  }
}

TEST(adler32, byte_ranges) {
  constexpr std::string_view text = "123456789";
  auto const expected = adler32_compute(std::as_bytes(std::span(text)));
  EXPECT_EQ(adler32_compute(text), expected);
  EXPECT_EQ(adler32_compute(std::list<char>(text.begin(), text.end())), expected);
  EXPECT_EQ(adler32_compute(std::vector<unsigned char>(text.begin(), text.end())), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  auto const data = random_bytes(203, 4);
  EXPECT_EQ(adler32_compute(std::list<std::byte>(data.begin(), data.end())), reference(data));
  EXPECT_EQ(adler32_update(adler32_state{}, std::list<std::byte>(data.begin(), data.end())), adler32_update(adler32_state{}, data));
}

// Sums from 65521 count modulo 65521, at compile time and at run time; results are canonical.
TEST(adler32, edge_states) {
  constexpr adler32_state maximum{.sum1 = 0xFFFF, .sum2 = 0xFFFF};
  constexpr adler32_state equivalent{.sum1 = 14, .sum2 = 14};
  static_assert(adler32_finalize(maximum) == adler32_finalize(equivalent));
  static_assert(adler32_update(maximum, std::span<std::byte const>{}) == equivalent);
  static_assert(adler32_finalize(adler32_state{.sum1 = 65521, .sum2 = 65521}) == 0);
  EXPECT_EQ(adler32_update(maximum, std::span<std::byte const>{}), equivalent);
  EXPECT_EQ(adler32_finalize(adler32_state{.sum1 = 65521, .sum2 = 65521}), 0U);

  auto const data = random_bytes(37, 6);
  auto const message = std::span<std::byte const>(data);
  for(std::uint32_t sum = 65516; sum <= 0xFFFF; ++sum) {
    adler32_state const state{.sum1 = static_cast<std::uint16_t>(sum), .sum2 = static_cast<std::uint16_t>(sum)};
    for(std::size_t size = 0; size <= data.size(); ++size) {
      auto const result = adler32_update(state, message.first(size));
      ASSERT_EQ(adler32_finalize(result), reference(message.first(size), sum, sum)) << "sum " << sum << ", size " << size;
      ASSERT_LT(result.sum1, 65521U);
      ASSERT_LT(result.sum2, 65521U);
    }
  }
}

} // namespace
