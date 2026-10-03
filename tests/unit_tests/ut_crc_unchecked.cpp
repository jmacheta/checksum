// Built with NDEBUG: violated preconditions give clamped or masked results instead of failing an assert.

#include "crc_test_support.hpp"

#include <checksum/crc.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>

#if !defined(NDEBUG)
#error "ut_crc_unchecked.cpp must be compiled with NDEBUG"
#endif

namespace {

TEST(CrcUnchecked, ViolationsAreClampedOrMasked) {
  auto const &engine = checksum::crc_engine_for<crc_test::model_parameters<6>>; // CRC-15/CAN
  std::array<std::byte, 2> const data{std::byte{0x12}, std::byte{0x34}};
  auto const initial = engine.initial();
  EXPECT_EQ(engine.update(0x9234U, data), engine.update(0x1234, data));
  EXPECT_EQ(engine.finalize(0xFFFF), engine.finalize(0x7FFF));
  EXPECT_EQ(engine.update(initial, data, 17), engine.update(initial, data));
  EXPECT_EQ(engine.update(initial, data, 1000), engine.update(initial, data));
  EXPECT_EQ(engine.update(initial, std::span<std::byte const>{}, 3), initial);
}

} // namespace
