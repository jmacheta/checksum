// Internet checksum: published examples, an independent model, splits and ranges.

#include <checksum/internet.hpp>

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

template <std::size_t Size> constexpr std::array<std::byte, Size> bytes(std::array<unsigned, Size> const &values) {
  std::array<std::byte, Size> result{};
  for(std::size_t index = 0; index < Size; ++index) {
    result[index] = static_cast<std::byte>(values[index]);
  }
  return result;
}

// Example of RFC 1071, section 3: one's complement sum 0xDDF2.
constexpr auto rfc1071_example = bytes<8>({0x00, 0x01, 0xF2, 0x03, 0xF4, 0xF5, 0xF6, 0xF7});

// IPv4 header with its checksum field (bytes 10 and 11) zeroed; the checksum is 0xB861.
constexpr auto ipv4_header =
    bytes<20>({0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x00, 0x01, 0xC0, 0xA8, 0x00, 0xC7});

// Independent model: the one's complement sum is the plain sum of the big-endian words modulo 0xFFFF, written 0xFFFF
// rather than 0 unless every word is 0.
std::uint16_t reference(std::span<std::byte const> data) {
  std::uint64_t total = 0;
  for(std::size_t index = 0; index < data.size(); ++index) {
    total += std::to_integer<std::uint64_t>(data[index]) * (index % 2 == 0 ? 256U : 1U);
  }
  auto const sum = total == 0 ? 0U : static_cast<unsigned>(((total - 1) % 0xFFFFU) + 1);
  return static_cast<std::uint16_t>(~sum);
}

std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed) {
  std::mt19937 generator(seed);
  std::vector<std::byte> data(size);
  for(auto &byte : data) {
    byte = static_cast<std::byte>(generator());
  }
  return data;
}

static_assert(internet_finalize(internet_update({}, rfc1071_example)) == 0x220D);
static_assert(internet_update({}, rfc1071_example) == internet_state{.sum = 0xDDF2, .odd = false});
static_assert(internet_compute(ipv4_header) == 0xB861);
static_assert(internet_compute(std::span<std::byte const>{}) == 0xFFFF);
static_assert(internet_compute(std::string_view("\x01")) == 0xFEFF);

TEST(internet, published_examples) {
  EXPECT_EQ(internet_update({}, rfc1071_example), (internet_state{.sum = 0xDDF2, .odd = false}));
  EXPECT_EQ(internet_compute(rfc1071_example), 0x220D);
  EXPECT_EQ(internet_compute(ipv4_header), 0xB861);
}

TEST(internet, zero_and_all_ones) {
  EXPECT_EQ(internet_compute(std::span<std::byte const>{}), 0xFFFF);
  EXPECT_EQ(internet_compute(std::vector<std::byte>(1000)), 0xFFFF);
  // Sums to 0xFFFF, the non-zero representation of zero.
  EXPECT_EQ(internet_compute(std::vector<std::byte>(1000, std::byte{0xFF})), 0x0000);
  EXPECT_EQ(internet_compute(bytes<4>({0x00, 0x01, 0xFF, 0xFE})), 0x0000);
}

TEST(internet, message_with_its_checksum_sums_to_zero) {
  auto header = ipv4_header;
  std::uint16_t const value = internet_compute(header);
  header[10] = static_cast<std::byte>(value >> 8U);
  header[11] = static_cast<std::byte>(value & 0xFFU);
  EXPECT_EQ(internet_compute(header), 0);
}

// RFC 1624 with the public API: start from the complemented checksum, fold the complemented old field and the new field.
TEST(internet, field_update) {
  auto header = ipv4_header;
  std::uint16_t const old_checksum = internet_compute(header);
  auto const old_field = bytes<2>({0x40, 0x11}); // TTL 64, UDP
  auto const new_field = bytes<2>({0x3F, 0x11}); // TTL 63
  header[8] = new_field[0];
  internet_state state{.sum = static_cast<std::uint16_t>(~old_checksum)};
  state = internet_update(state, bytes<2>({~0x40U & 0xFFU, ~0x11U & 0xFFU}));
  state = internet_update(state, new_field);
  EXPECT_EQ(old_field[0], ipv4_header[8]);
  EXPECT_EQ(internet_finalize(state), internet_compute(header));
}

TEST(internet, odd_length) {
  EXPECT_EQ(internet_compute(bytes<1>({0x01})), 0xFEFF);
  EXPECT_EQ(internet_compute(bytes<3>({0x12, 0x34, 0x56})), static_cast<std::uint16_t>(~(0x1234 + 0x5600)));
  EXPECT_TRUE(internet_update({}, bytes<3>({0x12, 0x34, 0x56})).odd);
}

// All lengths past the unrolled loop, its tail and the thresholds of every CPU kernel, at every alignment of a word.
TEST(internet, matches_model) {
  auto const data = random_bytes(1100 + 16, 1);
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 0; size <= 1100; ++size) {
      auto const message = std::span(data).subspan(offset, size);
      ASSERT_EQ(internet_compute(message), reference(message)) << "offset " << offset << ", size " << size;
    }
  }
}

TEST(internet, matches_model_with_carries) {
  // High bytes force carries out of every word the loop adds.
  std::vector<std::byte> data = random_bytes(1 << 16, 2);
  for(std::size_t index = 0; index < data.size(); index += 3) {
    data[index] = std::byte{0xFF};
  }
  EXPECT_EQ(internet_compute(data), reference(data));
  // Every start address modulo 4, and more than one pass of the vector kernels.
  std::vector<std::byte> const ones((5 << 20) + 4, std::byte{0xFF});
  for(std::size_t offset = 0; offset < 4; ++offset) {
    EXPECT_EQ(internet_compute(std::span(ones).subspan(offset, (1 << 20) + 1)), 0x00FF) << "offset " << offset;
  }
  EXPECT_EQ(internet_compute(std::span(ones).first((5 << 20) + 1)), 0x00FF);
}

TEST(internet, split_anywhere) {
  auto const data = random_bytes(80, 3);
  std::uint16_t const whole = internet_compute(data);
  auto const message = std::span<std::byte const>(data);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    for(std::size_t second = first; second <= data.size(); ++second) {
      internet_state state = internet_update({}, message.first(first));
      state = internet_update(state, message.subspan(first, second - first));
      state = internet_update(state, message.subspan(second));
      ASSERT_EQ(internet_finalize(state), whole) << "split at " << first << " and " << second;
    }
  }
}

// An odd running state entering chunks long enough for the CPU kernel.
TEST(internet, split_before_long_chunks) {
  auto const data = random_bytes(1200, 5);
  std::uint16_t const whole = internet_compute(data);
  auto const message = std::span<std::byte const>(data);
  for(std::size_t const first : {1U, 3U, 63U, 255U, 257U, 511U, 513U, 939U}) {
    internet_state const state = internet_update(internet_update({}, message.first(first)), message.subspan(first));
    ASSERT_EQ(internet_finalize(state), whole) << "split at " << first;
  }
}

constexpr std::size_t constant_message_size = 300;

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

// Every prefix computed at compile time, plus one split at an odd offset.
TEST(internet, constant_evaluation_matches_run_time) {
  static constexpr auto prefixes = [] {
    std::array<std::uint16_t, constant_message_size + 1> result{};
    for(std::size_t size = 0; size <= constant_message_size; ++size) {
      result[size] = internet_compute(std::span(constant_message).first(size));
    }
    return result;
  }();
  static constexpr std::uint16_t split =
      internet_finalize(internet_update(internet_update({}, std::span(constant_message).first(7)), std::span(constant_message).subspan(7)));
  auto const message = std::span<std::byte const>(constant_message);
  for(std::size_t size = 0; size <= constant_message_size; ++size) {
    ASSERT_EQ(prefixes[size], internet_compute(message.first(size))) << "size " << size;
  }
  EXPECT_EQ(split, internet_compute(message));
}

TEST(internet, byte_ranges) {
  constexpr std::string_view text = "123456789";
  std::uint16_t const expected = internet_compute(std::as_bytes(std::span(text)));
  EXPECT_EQ(internet_compute(text), expected);
  EXPECT_EQ(internet_compute(std::list<char>(text.begin(), text.end())), expected);
  EXPECT_EQ(internet_compute(std::vector<unsigned char>(text.begin(), text.end())), expected);
  static_assert(internet_compute(std::string_view("123456789")) == 0xF62A);
  // Longer than one 64-byte chunk of a non-contiguous range.
  auto const data = random_bytes(200, 4);
  EXPECT_EQ(internet_compute(std::list<std::byte>(data.begin(), data.end())), internet_compute(data));
}

} // namespace
