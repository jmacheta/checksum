#include "crc_test_support.hpp"

#include <checksum/crc.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace {

using namespace std::literals;
using checksum::crc_accumulator;
using checksum::crc_engine;
using checksum::crc_engine_for;
using checksum::crc_lut_byte;
using checksum::crc_lut_nibble;
using checksum::crc_lut_none;
using checksum::crc_lut_sliced;
using crc_test::crc_model;
using crc_test::for_each_engine;
using crc_test::models;

inline constexpr auto crc32 = crc_test::model_parameters<9>; // CRC-32/ISO-HDLC

// CTAD deduces the engine's types, with crc_lut_none as the default strategy. The accumulator is trivially copyable,
// holds only an engine pointer and a state, and cannot bind to a temporary engine.
static_assert(std::is_same_v<decltype(crc_accumulator{crc_engine_for<crc32>}), crc_accumulator<std::uint32_t, crc_lut_none>>);
static_assert(std::is_same_v<decltype(crc_accumulator{crc_engine_for<crc32, crc_lut_nibble>}), crc_accumulator<std::uint32_t, crc_lut_nibble>>);
static_assert(std::is_same_v<crc_accumulator<std::uint32_t>, crc_accumulator<std::uint32_t, crc_lut_none>>);
static_assert(std::is_trivially_copyable_v<crc_accumulator<std::uint32_t>>);
struct pointer_and_state {
  void const *engine;
  std::uint64_t state;
};
static_assert(sizeof(crc_accumulator<std::uint64_t>) == sizeof(pointer_and_state));
static_assert(!std::is_constructible_v<crc_accumulator<std::uint32_t>, crc_engine<std::uint32_t> &&>);
static_assert(std::is_constructible_v<crc_accumulator<std::uint32_t>, crc_engine<std::uint32_t> &>);

inline constexpr checksum::crc_parameters my_crc{.polynomial = 0x8810, .initial_value = 0xFFFF};

// A custom CRC on a nibble table, fed in two chunks.
constexpr std::uint16_t usage_example() {
  crc_accumulator accumulator{crc_engine_for<my_crc, crc_lut_nibble>};
  accumulator.update("1234"sv).update("56789"sv);
  return accumulator.value();
}
static_assert(usage_example() == 0x29B1); // CRC-16/IBM-3740

// value() leaves the state unchanged and reset() restarts; bit-length updates mix with byte updates.
constexpr bool value_is_non_destructive() {
  auto const &engine = crc_engine_for<crc32, crc_lut_byte>;
  crc_accumulator accumulator{engine};
  accumulator.update("1234"sv);
  auto const state = accumulator.state();
  (void)accumulator.value();
  bool passed = accumulator.state() == state;
  accumulator.update("56789"sv);
  passed = passed && accumulator.value() == 0xCBF43926 && accumulator.value() == engine.finalize(accumulator.state());
  accumulator.reset();
  passed = passed && accumulator.state() == engine.initial();
  accumulator.update(std::array{std::byte{'1'}}, 8).update(std::array{std::byte{'2'}, std::byte{'3'}}, 16).update("456789"sv);
  return passed && accumulator.value() == 0xCBF43926;
}
static_assert(value_is_non_destructive());

// Every byte split of a 64-byte message, every bit split of its first 64 bits, and bit-by-bit updates equal compute().
TEST(CrcAccumulator, EqualsComputeForEverySplit) {
  auto const message = random_bytes(64, 99);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      for(std::size_t split = 0; split <= data.size(); ++split) {
        crc_accumulator accumulator{engine};
        accumulator.update(data.first(split)).update(data.subspan(split));
        ASSERT_EQ(accumulator.value(), engine.compute(data)) << model.name << " split " << split;
        ASSERT_EQ(accumulator.state(), engine.update(engine.initial(), data));
      }
      for(std::size_t split = 0; split <= 64; ++split) {
        auto const first = crc_test::extract_bits(data, model.reflect_input, 0, split);
        auto const second = crc_test::extract_bits(data, model.reflect_input, split, 64 - split);
        crc_accumulator accumulator{engine};
        accumulator.update(first, split).update(second, 64 - split);
        ASSERT_EQ(accumulator.value(), engine.compute(data, 64)) << model.name << " bit split " << split;
      }
      crc_accumulator accumulator{engine};
      for(std::size_t bit = 0; bit < 64; ++bit) {
        accumulator.update(crc_test::extract_bits(data, model.reflect_input, bit, 1), 1);
      }
      EXPECT_EQ(accumulator.value(), engine.compute(data, 64)) << model.name;
    });
  }
}

TEST(CrcAccumulator, ResetAndRanges) {
  auto const &sliced = crc_engine_for<crc32, crc_lut_sliced>;
  crc_accumulator accumulator{sliced};
  EXPECT_EQ(accumulator.state(), sliced.initial());
  accumulator.update(crc_test::check_text);
  EXPECT_EQ(accumulator.value(), 0xCBF43926U);
  EXPECT_EQ(accumulator.value(), 0xCBF43926U);
  accumulator.reset();
  EXPECT_EQ(accumulator.value(), sliced.compute(std::span<std::byte const>{}));
  std::array<unsigned char, 9> const digits{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  accumulator.update(digits);
  EXPECT_EQ(accumulator.value(), 0xCBF43926U);

  auto copy = accumulator; // trivially copyable value
  copy.update("x"sv);
  EXPECT_EQ(accumulator.value(), 0xCBF43926U);
  EXPECT_NE(copy.value(), accumulator.value());
}

} // namespace
