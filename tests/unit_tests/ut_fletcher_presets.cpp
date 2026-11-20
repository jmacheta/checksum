// Fletcher presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/fletcher.hpp>
#include <checksum/isa.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// The largest sums of Width.
template <unsigned Width> constexpr auto largest_sum = static_cast<fletcher_state<Width>::sum_type>(-1);
template <unsigned Width> constexpr fletcher_state<Width> largest_state{.sum1 = largest_sum<Width>, .sum2 = largest_sum<Width>};

// Calls check(fletcher_engine<Width, Preset>{}, fletcher_engine<Width>{}, largest_state<Width>) for every width.
template <isa Preset, class Check> void for_each_width(Check check) {
  check(fletcher_engine<16, Preset>{}, fletcher_engine<16>{}, largest_state<16>);
  check(fletcher_engine<32, Preset>{}, fletcher_engine<32>{}, largest_state<32>);
  check(fletcher_engine<64, Preset>{}, fletcher_engine<64>{}, largest_state<64>);
}

template <class Preset> using fletcher_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(fletcher_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset; the check values of "123456789".
TYPED_TEST(fletcher_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(fletcher_compute<16, preset>("123456789"sv) == 0x1EDE);
  static_assert(fletcher_compute<32, preset>("123456789"sv) == 0xDF09D509);
  static_assert(fletcher_compute<64, preset>("123456789"sv) == 0x0D0803376C6A689F);
  static_assert(engine_checks::continued(fletcher_engine<32, preset>{}, fletcher_compute<32>("12345678"sv), "9"sv) == 0xDF09D509);
  static_assert(engine_checks::continued(fletcher_engine<64, preset>{}, fletcher_compute<64>("12345678"sv), "9"sv) == 0x0D0803376C6A689F);
  EXPECT_EQ((fletcher_compute<16, preset>("123456789"sv)), 0x1EDE);
  auto const buffer = random_bytes(1003, 4);
  for_each_width<preset>([&](auto const &engine, auto const &generic, auto const & /*largest*/) {
    EXPECT_EQ(engine.compute(at(buffer, 3, 1000)), generic.compute(at(buffer, 3, 1000)));
  });
}

TYPED_TEST(fletcher_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(2048 + 8, 1);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic, auto const & /*largest*/) { engine_checks::check_every_size(engine, generic, buffer); });
}

TYPED_TEST(fletcher_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic, auto const & /*largest*/) { engine_checks::check_large_sizes(engine, generic, buffer); });
}

TYPED_TEST(fletcher_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic, auto const & /*largest*/) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

// From the initial and the largest sums: the inputs closest to overflowing the deferred sums.
TYPED_TEST(fletcher_presets, match_generic_for_all_ones) {
  std::vector<std::byte> const ones((512UZ << 10U) + 64, std::byte{0xFF});
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic, auto const &largest) { engine_checks::check_all_ones(engine, generic, ones, largest); });
}

} // namespace
