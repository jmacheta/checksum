#include "crc_test_support.hpp"

#include <checksum/crc.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace {

using checksum::crc_engine;
using checksum::crc_parameters;
using checksum::crc_polynomial;
using checksum::crc_register;
using checksum::crc_register_for;

inline constexpr crc_parameters my_crc{.polynomial = 0xC002, .initial_value = 0xFFFF, .reflect_input = true, .reflect_output = true};
static_assert(my_crc.polynomial.width() == 16);
static_assert(my_crc.polynomial.normal_form() == 0x8005);
static_assert(my_crc.initial_value == 0xFFFF);
static_assert(my_crc.reflect_input && my_crc.reflect_output);
static_assert(my_crc.final_xor == 0);

// Defaults: initial value 0, no reflection, final XOR 0.
inline constexpr crc_parameters defaults{.polynomial = 0x83};
static_assert(defaults.initial_value == 0 && !defaults.reflect_input && !defaults.reflect_output && defaults.final_xor == 0);

static_assert(std::is_aggregate_v<crc_parameters>);
static_assert(sizeof(crc_parameters) == 32);
static_assert(std::is_copy_assignable_v<crc_parameters>);
static_assert(std::is_trivially_copy_constructible_v<crc_parameters> && std::is_trivially_destructible_v<crc_parameters>);
static_assert(!std::is_default_constructible_v<crc_parameters>);

// Structural type: equal values give the same template specialization.
template <crc_parameters Parameters> inline constexpr unsigned width_of = Parameters.polynomial.width();
static_assert(width_of<my_crc> == 16);
static_assert(crc_test::same_object(
    &width_of<my_crc>, &width_of<crc_parameters{.polynomial = 0xC002, .initial_value = 0xFFFF, .reflect_input = true, .reflect_output = true}>));

static_assert(crc_register<std::uint8_t> && crc_register<std::uint16_t> && crc_register<std::uint32_t> && crc_register<std::uint64_t>);
static_assert(!crc_register<int> && !crc_register<std::int32_t> && !crc_register<bool> && !crc_register<std::uint32_t const>);

static_assert(std::is_same_v<crc_register_for<1>, std::uint8_t>);
static_assert(std::is_same_v<crc_register_for<8>, std::uint8_t>);
static_assert(std::is_same_v<crc_register_for<9>, std::uint16_t>);
static_assert(std::is_same_v<crc_register_for<16>, std::uint16_t>);
static_assert(std::is_same_v<crc_register_for<17>, std::uint32_t>);
static_assert(std::is_same_v<crc_register_for<32>, std::uint32_t>);
static_assert(std::is_same_v<crc_register_for<33>, std::uint64_t>);
static_assert(std::is_same_v<crc_register_for<64>, std::uint64_t>);

template <unsigned Width>
concept has_register_for = requires { typename crc_register_for<Width>; };
static_assert(!has_register_for<0> && !has_register_for<65>);

// create() rejects an initial value or final XOR wider than the CRC.
TEST(CrcParameters, EngineCreateValidatesInitialValueAndFinalXor) {
  for(unsigned width = 1; width <= 64; ++width) {
    auto const polynomial = *crc_polynomial::create(1, width);
    auto const mask = width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1;
    EXPECT_TRUE(crc_engine<std::uint64_t>::create({.polynomial = polynomial, .initial_value = mask, .final_xor = mask})) << width;
    if(width < 64) {
      EXPECT_FALSE(crc_engine<std::uint64_t>::create({.polynomial = polynomial, .initial_value = mask + 1})) << width;
      EXPECT_FALSE(crc_engine<std::uint64_t>::create({.polynomial = polynomial, .final_xor = mask + 1})) << width;
      EXPECT_FALSE(crc_engine<std::uint64_t>::create({.polynomial = polynomial, .initial_value = ~std::uint64_t{0}})) << width;
    }
  }
}

TEST(CrcParameters, Equality) {
  auto const polynomial = *crc_polynomial::create(0x07, 8);
  crc_parameters const base{.polynomial = polynomial};
  EXPECT_EQ(base, (crc_parameters{.polynomial = polynomial}));
  EXPECT_NE(base, (crc_parameters{.polynomial = polynomial, .initial_value = 1}));
  EXPECT_NE(base, (crc_parameters{.polynomial = polynomial, .reflect_input = true}));
  EXPECT_NE(base, (crc_parameters{.polynomial = polynomial, .reflect_output = true}));
  EXPECT_NE(base, (crc_parameters{.polynomial = polynomial, .final_xor = 1}));
  EXPECT_NE(base, (crc_parameters{.polynomial = *crc_polynomial::create(0x07, 9)}));
}

} // namespace
