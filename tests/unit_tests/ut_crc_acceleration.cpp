// crc_lut_sliced and crc_lut_braided must match crc_lut_byte for every catalog parameter set, on the smallest register
// and on std::uint64_t, one-shot and split in two. The lengths and start offsets cross every size threshold and
// alignment of the accelerated folding kernels (16-byte blocks, wide loop from 256 bytes).
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include "crc_catalog_checks.hpp"
#include "crc_test_support.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace {

using namespace checksum;

constexpr std::size_t longest = 4096;

// Every start position within a 16-byte block.
constexpr std::size_t offsets = 16;

// Longer messages run at offsets 0 and 15 only, which keeps the test short in QEMU.
constexpr std::size_t every_offset_length = 256;

// Every length 0..640, then random lengths up to longest.
std::vector<std::size_t> lengths() {
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

template <class Strategy, crc_parameters Parameters> void check_against_byte() {
  using smallest_register = crc_register_for<Parameters.polynomial.width()>;
  static std::vector<std::byte> const message = crc_test::random_bytes(longest + offsets, 0x5EED);
  static std::vector<std::size_t> const message_lengths = lengths();
  // Static storage: engines of up to 32 KiB in each of the 112 instantiations would overflow the stack in sanitizer builds.
  static auto const reference = *crc_engine<smallest_register, crc_lut_byte>::create(Parameters);
  static auto const smallest = *crc_engine<smallest_register, Strategy>::create(Parameters);
  static auto const wide = *crc_engine<std::uint64_t, Strategy>::create(Parameters);
  for(std::size_t const length : message_lengths) {
    std::size_t const step = length <= every_offset_length ? 1 : offsets - 1;
    for(std::size_t offset = 0; offset < offsets; offset += step) {
      auto const data = std::span<std::byte const>(message).subspan(offset, length);
      auto const expected = reference.compute(data);
      ASSERT_EQ(smallest.compute(data), expected) << "length " << length << ", offset " << offset;
      ASSERT_EQ(wide.compute(data), expected) << "length " << length << ", offset " << offset;
      std::size_t const split = length / 3;
      ASSERT_EQ(smallest.finalize(smallest.update(smallest.update(smallest.initial(), data.first(split)), data.subspan(split))), expected)
          << "length " << length << ", offset " << offset << ", split " << split;
    }
  }
}

template <class Strategy> struct crc_acceleration_typed : ::testing::Test {};

using folding_strategies = ::testing::Types<crc_lut_sliced, crc_lut_braided>;
TYPED_TEST_SUITE(crc_acceleration_typed, folding_strategies);

template <class Strategy, std::size_t... Index> void check_every_set(std::index_sequence<Index...> /*indices*/) {
  (
      [] {
        SCOPED_TRACE(crc_test::catalog_checks[Index].name);
        check_against_byte<Strategy, crc_test::catalog_checks[Index].parameters>();
      }(),
      ...);
}

TYPED_TEST(crc_acceleration_typed, MatchesByteTableForEveryCatalogSet) {
  check_every_set<TypeParam>(std::make_index_sequence<crc_test::catalog_checks.size()>{});
}

} // namespace
