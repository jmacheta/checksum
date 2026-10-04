// Fletcher checksums: published examples, an independent model, splits, ranges and edge states.

#include <checksum/fletcher.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <list>
#include <random>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// Examples of the Wikipedia article "Fletcher's checksum", checked against the reference model below.
static_assert(fletcher_compute<16>("abcde"sv) == 0xC8F0);
static_assert(fletcher_compute<16>("abcdef"sv) == 0x2057);
static_assert(fletcher_compute<16>("abcdefgh"sv) == 0x0627);
static_assert(fletcher_compute<32>("abcde"sv) == 0xF04FC729);
static_assert(fletcher_compute<32>("abcdef"sv) == 0x56502D2A);
static_assert(fletcher_compute<32>("abcdefgh"sv) == 0xEBE19591);
static_assert(fletcher_compute<64>("abcde"sv) == 0xC8C6C527646362C6);
static_assert(fletcher_compute<64>("abcdef"sv) == 0xC8C72B276463C8C6);
static_assert(fletcher_compute<64>("abcdefgh"sv) == 0x312E2B28CCCAC8C6);
static_assert(fletcher_compute<32>(std::span<std::byte const>{}) == 0);

// The aliases of each width are the generic forms.
static_assert(std::same_as<fletcher16_state, fletcher_state<16>> && std::same_as<fletcher32_state, fletcher_state<32>> &&
              std::same_as<fletcher64_state, fletcher_state<64>>);
static_assert(fletcher16_compute("abcde"sv) == 0xC8F0 && fletcher32_compute("abcde"sv) == 0xF04FC729 &&
              fletcher64_compute("abcde"sv) == 0xC8C6C527646362C6);

// Independent model: whole blocks read byte by byte with zero padding, each sum reduced with %.
template <unsigned Width> std::uint64_t reference(std::span<std::byte const> data) {
  constexpr std::size_t block_size = Width / 16;
  constexpr std::uint64_t modulus = (std::uint64_t{1} << (Width / 2)) - 1;
  std::uint64_t sum1 = 0;
  std::uint64_t sum2 = 0;
  for(std::size_t start = 0; start < data.size(); start += block_size) {
    std::uint64_t block = 0;
    for(std::size_t index = 0; index < block_size && start + index < data.size(); ++index) {
      block += std::to_integer<std::uint64_t>(data[start + index]) << (8 * index);
    }
    sum1 = (sum1 + block) % modulus;
    sum2 = (sum2 + sum1) % modulus;
  }
  return (sum2 << (Width / 2)) | sum1;
}

std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed) {
  std::mt19937 generator(seed);
  std::vector<std::byte> data(size);
  for(auto &byte : data) {
    byte = static_cast<std::byte>(generator());
  }
  return data;
}

template <class Width> class fletcher : public testing::Test {};

using widths = testing::Types<std::integral_constant<unsigned, 16>, std::integral_constant<unsigned, 32>, std::integral_constant<unsigned, 64>>;
TYPED_TEST_SUITE(fletcher, widths);

TEST(fletcher_vectors, published_examples) {
  for(auto const text : {"abcde"sv, "abcdef"sv, "abcdefgh"sv}) {
    auto const message = std::as_bytes(std::span(text));
    EXPECT_EQ(fletcher_compute<16>(message), reference<16>(message)) << text;
    EXPECT_EQ(fletcher_compute<32>(message), reference<32>(message)) << text;
    EXPECT_EQ(fletcher_compute<64>(message), reference<64>(message)) << text;
  }
  EXPECT_EQ(fletcher_compute<16>("abcde"sv), 0xC8F0);
  EXPECT_EQ(fletcher_compute<16>("abcdef"sv), 0x2057);
  EXPECT_EQ(fletcher_compute<16>("abcdefgh"sv), 0x0627);
  EXPECT_EQ(fletcher_compute<32>("abcde"sv), 0xF04FC729);
  EXPECT_EQ(fletcher_compute<32>("abcdef"sv), 0x56502D2A);
  EXPECT_EQ(fletcher_compute<32>("abcdefgh"sv), 0xEBE19591);
  EXPECT_EQ(fletcher_compute<64>("abcde"sv), 0xC8C6C527646362C6);
  EXPECT_EQ(fletcher_compute<64>("abcdef"sv), 0xC8C72B276463C8C6);
  EXPECT_EQ(fletcher_compute<64>("abcdefgh"sv), 0x312E2B28CCCAC8C6);
}

TEST(fletcher_vectors, aliases) {
  auto const data = random_bytes(300, 8);
  auto const message = std::span<std::byte const>(data);
  EXPECT_EQ(fletcher16_compute(message), fletcher_compute<16>(message));
  EXPECT_EQ(fletcher32_compute(message), fletcher_compute<32>(message));
  EXPECT_EQ(fletcher64_compute(message), fletcher_compute<64>(message));
  EXPECT_EQ(fletcher16_compute(data), fletcher_compute<16>(message));
  EXPECT_EQ(fletcher32_compute(std::list<std::byte>(data.begin(), data.end())), fletcher_compute<32>(message));
  EXPECT_EQ(fletcher64_compute("123456789"sv), fletcher_compute<64>("123456789"sv));
}

// All lengths past the kernel thresholds and a few kernel blocks, at every alignment of a vector.
TYPED_TEST(fletcher, matches_model) {
  constexpr unsigned width = TypeParam::value;
  auto const data = random_bytes(2100 + 16, width);
  for(std::size_t offset = 0; offset < 16; ++offset) {
    for(std::size_t size = 0; size <= 2100; ++size) {
      auto const message = std::span<std::byte const>(data).subspan(offset, size);
      ASSERT_EQ(fletcher_compute<width>(message), reference<width>(message)) << "offset " << offset << ", size " << size;
    }
  }
}

// The largest blocks over several deferred reductions; the tail sizes leave an unfinished block.
TYPED_TEST(fletcher, all_ones) {
  constexpr unsigned width = TypeParam::value;
  for(std::size_t const size : {(std::size_t{4} << 20), (std::size_t{4} << 20) + 1, (std::size_t{4} << 20) + 3}) {
    std::vector<std::byte> const data(size, std::byte{0xFF});
    ASSERT_EQ(fletcher_compute<width>(data), reference<width>(data)) << "size " << size;
  }
}

// Around the chunk sizes of every kernel (1 KiB to 256 KiB), from every alignment of a word, with all-ones and random bytes and
// from sums equal to M.
TYPED_TEST(fletcher, chunk_boundaries) {
  constexpr unsigned width = TypeParam::value;
  using sum_type = fletcher_state<width>::sum_type;
  constexpr auto largest = static_cast<sum_type>(-1);
  constexpr std::size_t largest_chunk = std::size_t{256} << 10U;
  std::vector<std::byte> const ones((2 * largest_chunk) + 64, std::byte{0xFF});
  auto const random = random_bytes(ones.size(), 7);
  for(auto const &data : {ones, random}) {
    for(std::size_t const chunk :
        {std::size_t{1024}, std::size_t{2048}, std::size_t{4096}, std::size_t{5600}, std::size_t{16} << 10U, std::size_t{32} << 10U, largest_chunk}) {
      for(std::size_t const size : {chunk - 1, chunk, chunk + 1, chunk + 35, (2 * chunk) + 35}) {
        for(std::size_t offset = 0; offset < 4; ++offset) {
          auto const message = std::span<std::byte const>(data).subspan(offset, size);
          ASSERT_EQ(fletcher_compute<width>(message), reference<width>(message)) << "size " << size << ", offset " << offset;
          ASSERT_EQ(fletcher_update(fletcher_state<width>{.sum1 = largest, .sum2 = largest}, message),
                    fletcher_update(fletcher_state<width>{}, message))
              << "size " << size << ", offset " << offset;
        }
      }
    }
  }
}

// One deferred reduction of 23'726'745 blocks on 64-bit targets, from the largest sums. A block of all ones is 0 modulo M, while an
// overflow would add 2^64, which is 1 modulo M.
TEST(fletcher_vectors, all_ones_longest_run) {
  std::vector<std::byte> const data(std::size_t{48} << 20, std::byte{0xFF});
  fletcher_state<32> const largest{.sum1 = 0xFFFF, .sum2 = 0xFFFF};
  EXPECT_EQ(fletcher_finalize(fletcher_update(largest, data)), 0U);
}

TYPED_TEST(fletcher, split_anywhere) {
  constexpr unsigned width = TypeParam::value;
  auto const data = random_bytes(80, 3);
  auto const message = std::span<std::byte const>(data);
  fletcher_state<width> const whole = fletcher_update(fletcher_state<width>{}, message);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    for(std::size_t second = first; second <= data.size(); ++second) {
      fletcher_state<width> state = fletcher_update(fletcher_state<width>{}, message.first(first));
      state = fletcher_update(state, message.subspan(first, second - first));
      state = fletcher_update(state, message.subspan(second));
      ASSERT_EQ(state, whole) << "split at " << first << " and " << second;
    }
  }
}

// An unfinished block entering chunks long enough for the kernels and their chunk boundaries.
TYPED_TEST(fletcher, split_before_long_chunks) {
  constexpr unsigned width = TypeParam::value;
  auto const data = random_bytes(20000, 5);
  auto const message = std::span<std::byte const>(data);
  auto const whole = reference<width>(message);
  for(std::size_t const first : {1U, 2U, 3U, 5U, 63U, 255U, 257U, 1023U, 1501U, 4093U}) {
    fletcher_state<width> const state = fletcher_update(fletcher_update(fletcher_state<width>{}, message.first(first)), message.subspan(first));
    ASSERT_EQ(fletcher_finalize(state), whole) << "split at " << first;
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

// Every prefix computed at compile time, plus splits inside a block.
TYPED_TEST(fletcher, constant_evaluation_matches_run_time) {
  constexpr unsigned width = TypeParam::value;
  using value_type = fletcher_state<width>::value_type;
  static constexpr auto prefixes = [] {
    std::array<value_type, constant_message_size + 1> result{};
    for(std::size_t size = 0; size <= constant_message_size; ++size) {
      result[size] = fletcher_compute<width>(std::span(constant_message).first(size));
    }
    return result;
  }();
  static constexpr auto splits = [] {
    std::array<fletcher_state<width>, 4> result{};
    for(std::size_t first = 0; first < result.size(); ++first) {
      result[first] = fletcher_update(fletcher_update(fletcher_state<width>{}, std::span(constant_message).first(first + 5)),
                                      std::span(constant_message).subspan(first + 5));
    }
    return result;
  }();
  auto const message = std::span<std::byte const>(constant_message);
  for(std::size_t size = 0; size <= constant_message_size; ++size) {
    ASSERT_EQ(prefixes[size], fletcher_compute<width>(message.first(size))) << "size " << size;
  }
  for(std::size_t first = 0; first < splits.size(); ++first) {
    EXPECT_EQ(splits[first], fletcher_update(fletcher_update(fletcher_state<width>{}, message.first(first + 5)), message.subspan(first + 5)));
  }
}

TYPED_TEST(fletcher, byte_ranges) {
  constexpr unsigned width = TypeParam::value;
  constexpr std::string_view text = "123456789";
  auto const expected = fletcher_compute<width>(std::as_bytes(std::span(text)));
  EXPECT_EQ(fletcher_compute<width>(text), expected);
  EXPECT_EQ(fletcher_compute<width>(std::list<char>(text.begin(), text.end())), expected);
  EXPECT_EQ(fletcher_compute<width>(std::vector<unsigned char>(text.begin(), text.end())), expected);
  // Longer than one 64-byte chunk of a non-contiguous range, chunks ending inside a block.
  auto const data = random_bytes(203, 4);
  EXPECT_EQ(fletcher_compute<width>(std::list<std::byte>(data.begin(), data.end())), reference<width>(data));
  EXPECT_EQ(fletcher_update(fletcher_state<width>{}, std::list<std::byte>(data.begin(), data.end())), fletcher_update(fletcher_state<width>{}, data));
}

// M counts as 0 and the block offset is taken modulo the block size, at compile time and at run time, with messages long
// enough for the kernels.
TYPED_TEST(fletcher, edge_states) {
  constexpr unsigned width = TypeParam::value;
  using sum_type = fletcher_state<width>::sum_type;
  constexpr auto largest = static_cast<sum_type>(-1);
  constexpr std::size_t block_size = width / 16;
  auto const data = random_bytes(300, 6);
  auto const message = std::span<std::byte const>(data);

  constexpr fletcher_state<width> maximum{.sum1 = largest, .sum2 = largest, .block_offset = 0xFF};
  constexpr fletcher_state<width> equivalent{.sum1 = 0, .sum2 = 0, .block_offset = 0xFF % block_size};
  static_assert(fletcher_finalize(maximum) == fletcher_finalize(equivalent));
  static_assert(fletcher_update(maximum, std::span<std::byte const>{}) == fletcher_update(equivalent, std::span<std::byte const>{}));
  EXPECT_EQ(fletcher_finalize(maximum), 0U);
  for(std::size_t size = 0; size <= data.size(); ++size) {
    ASSERT_EQ(fletcher_update(maximum, message.first(size)), fletcher_update(equivalent, message.first(size))) << "size " << size;
  }

  // Every offset in a block, including offsets past the block size, from the largest canonical sums.
  constexpr auto canonical = static_cast<sum_type>(largest - 1);
  for(unsigned offset = 0; offset < 8; ++offset) {
    fletcher_state<width> const state{.sum1 = canonical, .sum2 = canonical, .block_offset = static_cast<std::uint8_t>(offset)};
    fletcher_state<width> const masked{.sum1 = canonical, .sum2 = canonical, .block_offset = static_cast<std::uint8_t>(offset % block_size)};
    auto const result = fletcher_update(state, message);
    EXPECT_EQ(result, fletcher_update(masked, message)) << "offset " << offset;
    EXPECT_EQ(fletcher_finalize(state), fletcher_finalize(masked)) << "offset " << offset;
    EXPECT_LT(result.sum1, largest);
    EXPECT_LT(result.sum2, largest);
    EXPECT_LT(result.block_offset, block_size);
  }
}

} // namespace
