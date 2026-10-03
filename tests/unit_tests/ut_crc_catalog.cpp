// Every catalog parameter set computes its check value on every built-in strategy, at run time on the smallest register
// and on std::uint64_t. At compile time, every set runs on crc_lut_none and a representative subset on every strategy.
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include "crc_catalog_checks.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace {

using namespace checksum;

inline constexpr std::array<std::byte, 9> check_message_bytes = {
    std::byte{'1'}, std::byte{'2'}, std::byte{'3'}, std::byte{'4'}, std::byte{'5'}, std::byte{'6'}, std::byte{'7'}, std::byte{'8'}, std::byte{'9'},
};

constexpr std::span<std::byte const> check_message() { return std::span<std::byte const>{check_message_bytes}; }

using crc_test::catalog_check;
using crc_test::catalog_checks;

static_assert(catalog_checks.size() == 112);

// 0 for a set that is not in the catalog.
constexpr std::uint64_t check_of(crc_parameters const &parameters) {
  for(catalog_check const &entry : catalog_checks) {
    if(entry.parameters == parameters) {
      return entry.check;
    }
  }
  return 0;
}

template <crc_parameters Parameters> consteval bool every_strategy_matches() {
  constexpr std::uint64_t check = check_of(Parameters);
  return check == crc_engine_for<Parameters, crc_lut_none>.compute(check_message()) &&
         check == crc_engine_for<Parameters, crc_lut_nibble>.compute(check_message()) &&
         check == crc_engine_for<Parameters, crc_lut_byte>.compute(check_message()) &&
         check == crc_engine_for<Parameters, crc_lut_sliced>.compute(check_message()) &&
         check == crc_engine_for<Parameters, crc_lut_braided>.compute(check_message());
}

template <std::size_t... Index> consteval bool every_set_matches(std::index_sequence<Index...> /*indices*/) {
  return ((crc_engine_for<catalog_checks[Index].parameters>.compute(check_message()) == catalog_checks[Index].check) && ...);
}

// Fits the compilers' default constant-evaluation limits.
static_assert(every_set_matches(std::make_index_sequence<catalog_checks.size()>{}));

// Covers each register type, both bit orders, widths below 8 and a reflected result with unreflected input.
static_assert(every_strategy_matches<crc3_gsm>());
static_assert(every_strategy_matches<crc5_usb>());
static_assert(every_strategy_matches<crc8_smbus>());
static_assert(every_strategy_matches<crc8_maxim_dow>());
static_assert(every_strategy_matches<crc12_umts>()); // input not reflected, result reflected
static_assert(every_strategy_matches<crc15_can>());
static_assert(every_strategy_matches<crc16_kermit>());
static_assert(every_strategy_matches<crc16_xmodem>());
static_assert(every_strategy_matches<crc24_openpgp>());
static_assert(every_strategy_matches<crc32_bzip2>());
static_assert(every_strategy_matches<crc32_iso_hdlc>());
static_assert(every_strategy_matches<crc40_gsm>());
static_assert(every_strategy_matches<crc64_ecma_182>());
static_assert(every_strategy_matches<crc64_xz>());

template <class Strategy, class Register> void check_entry(catalog_check const &entry) {
  using engine_type = crc_engine<Register, Strategy>;
  // On the heap: braided engines take up to 32 KiB.
  auto const engine = std::make_unique<std::optional<engine_type> const>(engine_type::create(entry.parameters));
  ASSERT_TRUE(engine->has_value()) << entry.name;
  EXPECT_EQ((*engine)->compute(check_message()), entry.check) << entry.name << ", " << sizeof(Register) * 8 << "-bit register";
}

template <class Strategy> struct crc_catalog_typed : ::testing::Test {};

using all_strategies = ::testing::Types<crc_lut_none, crc_lut_nibble, crc_lut_byte, crc_lut_sliced, crc_lut_braided>;
TYPED_TEST_SUITE(crc_catalog_typed, all_strategies);

TYPED_TEST(crc_catalog_typed, AllParameterSetsMatchCheckValue) {
  for(catalog_check const &entry : catalog_checks) {
    unsigned const width = entry.parameters.polynomial.width();
    if(width <= 8) {
      check_entry<TypeParam, std::uint8_t>(entry);
    } else if(width <= 16) {
      check_entry<TypeParam, std::uint16_t>(entry);
    } else if(width <= 32) {
      check_entry<TypeParam, std::uint32_t>(entry);
    }
    check_entry<TypeParam, std::uint64_t>(entry);
  }
}

// An alias is the same object as its canonical set, so both name the same engine.
static_assert(&crc32 == &crc32_iso_hdlc && &crc32c == &crc32_iscsi && &crc16_ccitt_false == &crc16_ibm_3740 && &crc16_x25 == &crc16_ibm_sdlc);

// Parameters are a template argument by value: an inline set equal to a catalog set names the same engine.
static_assert(crc_parameters{.polynomial = *crc_polynomial::create(0x07, 8)} == crc8_smbus);

TEST(CrcCatalogAlias, SameEngineObjectAsCanonical) {
  EXPECT_EQ(&crc_engine_for<crc32>, &crc_engine_for<crc32_iso_hdlc>);
  EXPECT_EQ(&crc_engine_for<crc32c>, &crc_engine_for<crc32_iscsi>);
  EXPECT_EQ(&crc_engine_for<crc16_ccitt_false>, &crc_engine_for<crc16_ibm_3740>);
  EXPECT_EQ((&crc_engine_for<crc16_x25, crc_lut_byte>), (&crc_engine_for<crc16_ibm_sdlc, crc_lut_byte>));
  EXPECT_EQ(&crc_engine_for<crc_parameters{.polynomial = *crc_polynomial::create(0x07, 8)}>, &crc_engine_for<crc8_smbus>);
}

} // namespace
