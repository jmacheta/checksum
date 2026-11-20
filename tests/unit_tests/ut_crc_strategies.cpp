// CRC strategies and registers: the strategy concept, engine sizes, registers too narrow or wider than the CRC, and a user strategy.

#include "crc_test_support.hpp"

#include <checksum/crc.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace {

using namespace std::literals;
using namespace checksum;
using crc_test::models;

inline constexpr auto crc32 = crc_test::model_parameters<9>;     // CRC-32/ISO-HDLC
inline constexpr auto crc15_can = crc_test::model_parameters<6>; // CRC-15/CAN
inline constexpr auto crc12_umts = crc_test::model_parameters<5>;

template <class Strategy>
concept every_register_strategy = crc_strategy_for<Strategy, std::uint8_t> && crc_strategy_for<Strategy, std::uint16_t> &&
                                  crc_strategy_for<Strategy, std::uint32_t> && crc_strategy_for<Strategy, std::uint64_t>;

static_assert(every_register_strategy<crc_lut_none> && every_register_strategy<crc_lut_nibble> && every_register_strategy<crc_lut_byte> &&
              every_register_strategy<crc_lut_sliced> && every_register_strategy<crc_lut_braided>);

// An engine is its table entries plus at most 64 bytes of prepared values.
template <class Engine> constexpr bool engine_size_is(std::size_t entries) { return sizeof(Engine) >= entries && sizeof(Engine) <= entries + 64; }
static_assert(sizeof(crc_engine<std::uint32_t, crc_lut_none>) <= 32);
static_assert(engine_size_is<crc_engine<std::uint64_t, crc_lut_none>>(0));
static_assert(engine_size_is<crc_engine<std::uint32_t, crc_lut_nibble>>(16 * sizeof(std::uint32_t)));
static_assert(engine_size_is<crc_engine<std::uint32_t, crc_lut_byte>>(1024));
static_assert(engine_size_is<crc_engine<std::uint64_t, crc_lut_byte>>(2048));
// The slicing-by-8 and braided tables also hold 20 x 8 bytes of folding constants. The braided table is the 8
// slicing-by-8 slices plus 8 braid slices.
static_assert(engine_size_is<crc_engine<std::uint64_t, crc_lut_sliced>>(16384 + 160));
static_assert(engine_size_is<crc_engine<std::uint64_t, crc_lut_braided>>(32768 + 160));
static_assert(!std::is_default_constructible_v<crc_engine<std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<crc_engine<std::uint32_t, crc_lut_byte>> && std::is_copy_assignable_v<crc_engine<std::uint32_t>>);

TEST(crc_strategies, create_rejects_too_narrow_register) {
  EXPECT_FALSE((crc_engine<std::uint8_t>::create(crc12_umts)));
  EXPECT_FALSE((crc_engine<std::uint16_t, crc_lut_sliced>::create(crc32)));
  EXPECT_FALSE((crc_engine<std::uint32_t, crc_lut_none>::create(crc_test::model_parameters<12>)));
  EXPECT_TRUE((crc_engine<std::uint16_t, crc_lut_nibble>::create(crc12_umts)));
  EXPECT_FALSE((crc_lut_byte::make_table<std::uint8_t>(crc12_umts)));
}

TEST(crc_strategies, wider_register_same_result) {
  auto const message = random_bytes(100, 3);
  auto const narrow = *crc_engine<std::uint16_t>::create(crc12_umts);
  auto const wide = *crc_engine<std::uint64_t, crc_lut_sliced>::create(crc12_umts);
  EXPECT_EQ(narrow.compute(message), wide.compute(message));
  EXPECT_EQ(narrow.initial(), wide.initial());
  EXPECT_EQ(narrow.update(narrow.initial(), message), wide.update(wide.initial(), message));
}

// A user strategy, not constexpr, supporting only std::uint32_t and the CRC-32 polynomial. Its table is the reflected
// input flag.
struct crc32_only {
  template <class Register> using table_type = bool;

  template <class Register>
    requires std::same_as<Register, std::uint32_t>
  static std::optional<table_type<Register>> make_table(crc_parameters const &parameters) noexcept;

  template <class Register>
    requires std::same_as<Register, std::uint32_t>
  static Register update(table_type<Register> const &reflected, Register state, std::span<std::byte const> data) noexcept;
};

static_assert(crc_strategy_for<crc32_only, std::uint32_t>);
static_assert(!crc_strategy_for<crc32_only, std::uint16_t>);
static_assert(!crc_strategy_for<crc32_only, std::uint64_t>);
static_assert(!every_register_strategy<crc32_only>);

TEST(crc_strategies, user_strategy) {
  auto const iso_hdlc = crc_engine<std::uint32_t, crc32_only>::create(crc32);
  auto const mpeg2 = crc_engine<std::uint32_t, crc32_only>::create(crc_test::model_parameters<10>);
  ASSERT_TRUE(iso_hdlc);
  ASSERT_TRUE(mpeg2);
  EXPECT_FALSE((crc_engine<std::uint32_t, crc32_only>::create(crc15_can)));
  EXPECT_EQ(iso_hdlc->compute("123456789"sv), 0xCBF43926U);
  EXPECT_EQ(mpeg2->compute("123456789"sv), 0x0376E6E7U);
  auto const message = random_bytes(300, 8);
  for(std::size_t bits = 0; bits <= message.size() * 8; bits += 7) {
    EXPECT_EQ(iso_hdlc->compute(message, bits), crc_test::reference_crc(models[9], message, bits));
    EXPECT_EQ(mpeg2->compute(message, bits), crc_test::reference_crc(models[10], message, bits));
  }
}

template <class Register>
  requires std::same_as<Register, std::uint32_t>
std::optional<crc32_only::table_type<Register>> crc32_only::make_table(crc_parameters const &parameters) noexcept {
  if(parameters.polynomial != *crc_polynomial::create(0x04C11DB7, 32)) {
    return std::nullopt;
  }
  return parameters.reflect_input;
}

template <class Register>
  requires std::same_as<Register, std::uint32_t>
Register crc32_only::update(table_type<Register> const &reflected, Register state, std::span<std::byte const> data) noexcept {
  for(std::byte const byte : data) {
    if(reflected) {
      state ^= std::to_integer<std::uint32_t>(byte);
      for(int bit = 0; bit < 8; ++bit) {
        state = (state & 1U) != 0 ? (state >> 1U) ^ 0xEDB88320U : state >> 1U;
      }
    } else {
      state ^= std::to_integer<std::uint32_t>(byte) << 24U;
      for(int bit = 0; bit < 8; ++bit) {
        state = (state & 0x80000000U) != 0 ? (state << 1U) ^ 0x04C11DB7U : state << 1U;
      }
    }
  }
  return state;
}

} // namespace
