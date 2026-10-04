// crc_lut_sliced and crc_lut_braided must match crc_lut_byte for every catalog parameter set, on the smallest register
// and on std::uint64_t, one-shot and split in two. The first set of each code path runs every length across the
// thresholds and alignments of the kernels, the others the lengths around each threshold.
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include "crc_catalog_checks.hpp"
#include "crc_test_support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <vector>

namespace {

using namespace checksum;

constexpr std::size_t longest = 4096;

// Aligned and misaligned for every load width; no CRC loop branches on the address.
constexpr std::array<std::size_t, 2> offsets{0, 15};

// Every length 0..640 (two steps of the widest loop, 256 bytes, with every tail), then random lengths up to longest.
std::vector<std::size_t> every_length() {
  constexpr std::size_t exhaustive = 640;
  constexpr std::size_t random_count = 24;
  std::vector<std::size_t> result;
  for(std::size_t length = 0; length <= exhaustive; ++length) {
    result.push_back(length);
  }
  std::mt19937_64 generator(0xACCE1);
  std::uniform_int_distribution<std::size_t> distribution(exhaustive + 1, longest);
  for(std::size_t index = 0; index < random_count; ++index) {
    result.push_back(distribution(generator));
  }
  result.push_back(longest);
  return result;
}

// Every length up to 80 (the 8-byte words, CRC32 steps, 16-byte blocks, 40-byte braids and 64-byte steps), then each
// longer threshold ±1.
std::vector<std::size_t> threshold_lengths() {
  std::vector<std::size_t> result;
  for(std::size_t length = 0; length <= 80; ++length) {
    result.push_back(length);
  }
  for(std::size_t const size : {128U, 256U, 512U, 1024U}) {
    result.insert(result.end(), {size - 1, size, size + 1});
  }
  result.push_back(longest);
  return result;
}

// Parameter sets with the same register, bit order and (for CRC32 instructions) polynomial run the same code.
struct code_path {
  unsigned register_bits;
  bool reflected;
  std::uint64_t polynomial;

  bool operator==(code_path const &) const = default;
};

code_path code_path_of(crc_parameters const &parameters) {
  unsigned const width = parameters.polynomial.width();
  unsigned const register_bits = width <= 8 ? 8 : width <= 16 ? 16 : width <= 32 ? 32 : 64;
  return {.register_bits = register_bits,
          .reflected = parameters.reflect_input,
          .polynomial = register_bits == 32 && parameters.reflect_input ? parameters.polynomial.normal_form() : 0};
}

template <class Strategy, class Register>
void check_against_byte(crc_parameters const &parameters, std::span<std::byte const> message, std::span<std::size_t const> lengths) {
  // On the heap: a braided engine holds 32 KiB of tables.
  auto const reference = std::make_unique<crc_engine<Register, crc_lut_byte>>(*crc_engine<Register, crc_lut_byte>::create(parameters));
  auto const smallest = std::make_unique<crc_engine<Register, Strategy>>(*crc_engine<Register, Strategy>::create(parameters));
  auto const wide = std::make_unique<crc_engine<std::uint64_t, Strategy>>(*crc_engine<std::uint64_t, Strategy>::create(parameters));
  for(std::size_t const length : lengths) {
    for(std::size_t const offset : offsets) {
      auto const data = message.subspan(offset, length);
      auto const expected = reference->compute(data);
      ASSERT_EQ(smallest->compute(data), expected) << "length " << length << ", offset " << offset;
      ASSERT_EQ(wide->compute(data), expected) << "length " << length << ", offset " << offset;
      std::size_t const split = length / 3;
      ASSERT_EQ(smallest->finalize(smallest->update(smallest->update(smallest->initial(), data.first(split)), data.subspan(split))), expected)
          << "length " << length << ", offset " << offset << ", split " << split;
    }
  }
}

template <class Strategy> void check_set(crc_parameters const &parameters, std::span<std::byte const> message, std::span<std::size_t const> lengths) {
  switch(code_path_of(parameters).register_bits) {
  case 8:
    check_against_byte<Strategy, std::uint8_t>(parameters, message, lengths);
    break;
  case 16:
    check_against_byte<Strategy, std::uint16_t>(parameters, message, lengths);
    break;
  case 32:
    check_against_byte<Strategy, std::uint32_t>(parameters, message, lengths);
    break;
  default:
    check_against_byte<Strategy, std::uint64_t>(parameters, message, lengths);
  }
}

template <class Strategy> struct crc_acceleration_typed : ::testing::Test {};

using folding_strategies = ::testing::Types<crc_lut_sliced, crc_lut_braided>;
TYPED_TEST_SUITE(crc_acceleration_typed, folding_strategies);

TYPED_TEST(crc_acceleration_typed, MatchesByteTableForEveryCatalogSet) {
  auto const message = crc_test::random_bytes(longest + offsets.back(), 0x5EED);
  auto const all = every_length();
  auto const thresholds = threshold_lengths();
  std::vector<code_path> tested;
  for(crc_test::catalog_check const &set : crc_test::catalog_checks) {
    SCOPED_TRACE(set.name);
    code_path const path = code_path_of(set.parameters);
    bool const first = std::ranges::find(tested, path) == tested.end();
    if(first) {
      tested.push_back(path);
    }
    check_set<TypeParam>(set.parameters, message, first ? all : thresholds);
  }
}

} // namespace
