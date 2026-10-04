// Adler-32 checksum: published examples, an independent model, splits, ranges and edge states.

#include <checksum/adler32.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <list>
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

// All lengths past the kernel thresholds and a few kernel blocks, at every alignment of a word and two vector misalignments.
TEST(adler32, matches_model) {
  auto const data = random_bytes(2100 + 16, 32);
  for(std::size_t const offset : {0U, 1U, 2U, 3U, 7U, 15U}) {
    for(std::size_t size = 0; size <= 2100; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      ASSERT_EQ(adler32_compute(message), reference(message)) << "offset " << offset << ", size " << size;
    }
  }
}

// The largest bytes from the largest canonical sums, over many deferred reductions of 5'552 bytes on 32-bit targets.
TEST(adler32, all_ones) {
  adler32_state const largest{.sum1 = 65520, .sum2 = 65520};
  for(std::size_t const size : {(std::size_t{600} << 10), (std::size_t{600} << 10) + 1, (std::size_t{600} << 10) + 7, std::size_t{5552 * 120}}) {
    std::vector<std::byte> const data(size, std::byte{0xFF});
    ASSERT_EQ(adler32_compute(data), reference(data)) << "size " << size;
    ASSERT_EQ(adler32_finalize(adler32_update(largest, data)), reference(data, 65520, 65520)) << "size " << size;
  }
}

// Around the chunk sizes of every kernel (1 KiB to 256 KiB), from every alignment of a word, with all-ones and random bytes and
// from the largest canonical sums.
TEST(adler32, chunk_boundaries) {
  constexpr std::size_t largest_chunk = std::size_t{256} << 10U;
  std::vector<std::byte> const ones((2 * largest_chunk) + 64, std::byte{0xFF});
  auto const random = random_bytes(ones.size(), 7);
  for(auto const &data : {ones, random}) {
    for(std::size_t const chunk : {std::size_t{1024}, std::size_t{2048}, std::size_t{4096}, std::size_t{5600}, std::size_t{5760},
                                   std::size_t{16} << 10U, std::size_t{32} << 10U, largest_chunk}) {
      for(std::size_t const size : {chunk - 1, chunk, chunk + 1, chunk + 35, (2 * chunk) + 35}) {
        for(std::size_t offset = 0; offset < 4; ++offset) {
          auto const message = std::span<std::byte const>(data).subspan(offset, size);
          ASSERT_EQ(adler32_compute(message), reference(message)) << "size " << size << ", offset " << offset;
          ASSERT_EQ(adler32_finalize(adler32_update(adler32_state{.sum1 = 65520, .sum2 = 65520}, message)), reference(message, 65520, 65520))
              << "size " << size << ", offset " << offset;
        }
      }
    }
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

// Short and odd prefixes entering chunks long enough for the kernels and several deferred reductions.
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

// Pseudo-random bytes the constant evaluator can produce.
constexpr std::array<std::byte, constant_message_size> constant_message = [] {
  std::array<std::byte, constant_message_size> result{};
  fill_random(result);
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

// Sums from 65521 count modulo 65521, at compile time and at run time, with messages long enough for the kernels; results
// are canonical.
TEST(adler32, edge_states) {
  constexpr adler32_state maximum{.sum1 = 0xFFFF, .sum2 = 0xFFFF};
  constexpr adler32_state equivalent{.sum1 = 14, .sum2 = 14};
  static_assert(adler32_finalize(maximum) == adler32_finalize(equivalent));
  static_assert(adler32_update(maximum, std::span<std::byte const>{}) == equivalent);
  static_assert(adler32_finalize(adler32_state{.sum1 = 65521, .sum2 = 65521}) == 0);
  EXPECT_EQ(adler32_update(maximum, std::span<std::byte const>{}), equivalent);
  EXPECT_EQ(adler32_finalize(adler32_state{.sum1 = 65521, .sum2 = 65521}), 0U);

  auto const data = random_bytes(300, 6);
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

template <class Checksum>
concept resumable = requires(Checksum checksum, std::span<std::byte const> data) { adler32_update(checksum, data); };

// Only a std::uint32_t is a checksum; {} stays the empty state.
static_assert(resumable<std::uint32_t> && !resumable<int> && !resumable<std::uint64_t> && !resumable<std::uint16_t>);
static_assert(std::same_as<decltype(adler32_update({}, std::span<std::byte const>{})), adler32_state>);
static_assert(adler32_update(adler32_compute("12345"sv), "6789"sv) == 0x091E01DE);

// A stored checksum continues the message after any number of bytes, at compile time and at run time.
TEST(adler32, resume_from_checksum) {
  constexpr std::size_t constant_size = 64;
  static constexpr auto resumed = [] {
    auto const message = std::span(constant_message).first(constant_size);
    std::array<std::uint32_t, constant_size + 1> result{};
    for(std::size_t first = 0; first <= constant_size; ++first) {
      result[first] = adler32_update(adler32_compute(message.first(first)), message.subspan(first));
    }
    return result;
  }();
  for(std::uint32_t const value : resumed) {
    ASSERT_EQ(value, adler32_compute(std::span(constant_message).first(constant_size)));
  }

  auto const data = random_bytes(1100, 8);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference(message);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    ASSERT_EQ(adler32_update(adler32_compute(message.first(first)), message.subspan(first)), whole) << "split at " << first;
  }
  std::list<std::byte> const tail(data.begin() + 100, data.end());
  EXPECT_EQ(adler32_update(adler32_compute(message.first(100)), tail), whole);
  // Sums from 65521 count modulo 65521, as in the state.
  EXPECT_EQ(adler32_update(std::uint32_t{0xFFFFFFFF}, message), reference(message, 0xFFFF, 0xFFFF));
}

} // namespace
