#include <checksum/crc.hpp>

#include "crc_catalog_checks.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <random>
#include <type_traits>

namespace {

using checksum::crc_polynomial;

constexpr std::optional<crc_polynomial> ccitt = crc_polynomial::create(0x1021, 16);

static_assert(std::is_trivially_copy_constructible_v<crc_polynomial> && std::is_trivially_destructible_v<crc_polynomial>);
static_assert(!std::is_default_constructible_v<crc_polynomial>);
static_assert(sizeof(crc_polynomial) == 8);

// Koopman notation: bit i is the coefficient of x^(i + 1), the x^0 term is implicit. Every value is a polynomial.
static_assert(ccitt->value == 0x8810 && crc_polynomial{0x8810} == *ccitt);
static_assert(ccitt->width() == 16 && ccitt->normal_form() == 0x1021);
static_assert(crc_polynomial{0}.width() == 0 && crc_polynomial{0}.normal_form() == 0); // the polynomial 1
static_assert(crc_polynomial{1}.width() == 1 && crc_polynomial{1}.normal_form() == 1); // x + 1
static_assert(crc_polynomial{~std::uint64_t{0}}.width() == 64 && crc_polynomial{~std::uint64_t{0}}.normal_form() == ~std::uint64_t{0});
static_assert(std::is_convertible_v<std::uint64_t, crc_polynomial>);

constexpr std::uint64_t mask_of(unsigned width) { return width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1; }

TEST(CrcPolynomial, CreateAcceptsValidRange) {
  for(unsigned width = 1; width <= 64; ++width) {
    auto const max = mask_of(width);
    auto const polynomial = crc_polynomial::create(max, width);
    ASSERT_TRUE(polynomial.has_value()) << width;
    EXPECT_EQ(polynomial->normal_form(), max);
    EXPECT_EQ(polynomial->width(), width);
  }
  EXPECT_EQ(crc_polynomial::create(0, 0), crc_polynomial{0}) << "the polynomial 1";
}

TEST(CrcPolynomial, CreateRejectsInvalid) {
  EXPECT_FALSE(crc_polynomial::create(1, 65));
  EXPECT_FALSE(crc_polynomial::create(0, 1000));
  for(unsigned width = 1; width < 64; ++width) {
    EXPECT_FALSE(crc_polynomial::create(std::uint64_t{1} << width, width)) << width;
  }
  for(unsigned width = 1; width <= 64; ++width) {
    EXPECT_FALSE(crc_polynomial::create(0, width)) << "no x^0 term, width " << width;
  }
  EXPECT_FALSE(crc_polynomial::create(0x06, 8)) << "no x^0 term";
}

TEST(CrcPolynomial, Equality) {
  EXPECT_EQ(crc_polynomial::create(0x07, 8), crc_polynomial::create(0x07, 8));
  EXPECT_NE(crc_polynomial::create(0x07, 8), crc_polynomial::create(0x07, 9));
  EXPECT_NE(crc_polynomial::create(0x07, 8), crc_polynomial::create(0x05, 8));
}

// Every catalog polynomial converts to its normal form and width and back.
TEST(CrcPolynomial, CatalogPolynomialsRoundTrip) {
  for(crc_test::catalog_check const &entry : crc_test::catalog_checks) {
    crc_polynomial const polynomial = entry.parameters.polynomial;
    EXPECT_EQ(crc_polynomial::create(polynomial.normal_form(), polynomial.width()), polynomial) << entry.name;
  }
}

// Random polynomials of every width round-trip between the normal form and Koopman notation.
TEST(CrcPolynomial, RoundTrips) {
  std::mt19937_64 generator(1234);
  for(unsigned width = 1; width <= 64; ++width) {
    for(int round = 0; round < 32; ++round) {
      std::uint64_t const normal_form = (generator() & mask_of(width)) | 1U;
      auto const polynomial = *crc_polynomial::create(normal_form, width);
      ASSERT_EQ(polynomial.normal_form(), normal_form) << width;
      ASSERT_EQ(polynomial.width(), width);
      ASSERT_EQ(crc_polynomial{polynomial.value}, polynomial);
    }
  }
}

} // namespace
