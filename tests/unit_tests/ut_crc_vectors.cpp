// Every reference vector through every built-in strategy, on the smallest register and on std::uint64_t. Engines are
// created at run time, so the test instantiates once per strategy and register type, not once per vector.
#include <checksum/crc.hpp>

#include "crc_reference_vectors.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace {

using namespace checksum;

using crc_test::bit_lengths;
using crc_test::byte_lengths;
using crc_test::message_pool;
using crc_test::vector_set;
using crc_test::vector_sets;

std::span<std::byte const> message(std::size_t size) { return std::as_bytes(std::span{message_pool}.first(size)); }

// Byte vectors one-shot and through an accumulator fed in two chunks; bit vectors one-shot.
template <class Strategy, class Register> void check_set(vector_set const &set) {
  using engine_type = crc_engine<Register, Strategy>;
  // On the heap: braided engines are up to 32 KiB.
  auto const engine = std::make_unique<std::optional<engine_type> const>(engine_type::create(set.parameters));
  ASSERT_TRUE(engine->has_value()) << set.name;
  for(std::size_t index = 0; index < byte_lengths.size(); ++index) {
    std::size_t const length = byte_lengths[index];
    std::uint64_t const expected = set.byte_crcs[index];
    auto const data = message(length);
    EXPECT_EQ((*engine)->compute(data), expected) << set.name << ", " << length << " bytes";
    crc_accumulator accumulator{**engine};
    accumulator.update(data.first(length / 2)).update(data.subspan(length / 2));
    EXPECT_EQ(accumulator.value(), expected) << set.name << ", " << length << " bytes, accumulator";
  }
  for(std::size_t index = 0; index < bit_lengths.size(); ++index) {
    std::size_t const length = bit_lengths[index];
    EXPECT_EQ((*engine)->compute(message((length + 7) / 8), length), set.bit_crcs[index]) << set.name << ", " << length << " bits";
  }
}

template <class Strategy> void check_all_sets() {
  for(vector_set const &set : vector_sets) {
    unsigned const width = set.parameters.polynomial.width();
    if(width <= 8) {
      check_set<Strategy, std::uint8_t>(set);
    } else if(width <= 16) {
      check_set<Strategy, std::uint16_t>(set);
    } else if(width <= 32) {
      check_set<Strategy, std::uint32_t>(set);
    }
    check_set<Strategy, std::uint64_t>(set);
  }
}

template <class Strategy> struct crc_vectors_typed : ::testing::Test {};

using all_strategies = ::testing::Types<crc_lut_none, crc_lut_nibble, crc_lut_byte, crc_lut_sliced, crc_lut_braided>;
TYPED_TEST_SUITE(crc_vectors_typed, all_strategies);

TYPED_TEST(crc_vectors_typed, ReferenceVectors) { check_all_sets<TypeParam>(); }

} // namespace
