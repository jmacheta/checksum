// FNV-1a presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/fnv1a.hpp>
#include <checksum/isa.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <string_view>

namespace {

using namespace checksum;
using namespace std::literals;

// Calls check(fnv1a_engine<Width, Preset>{}, fnv1a_engine<Width>{}) for every width.
template <isa Preset, class Check> void for_each_width(Check check) {
  check(fnv1a_engine<32, Preset>{}, fnv1a_engine<32>{});
  check(fnv1a_engine<64, Preset>{}, fnv1a_engine<64>{});
}

template <class Preset> using fnv1a_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(fnv1a_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(fnv1a_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(fnv1a_compute<32, preset>("123456789"sv) == fnv1a_compute<32>("123456789"sv));
  static_assert(fnv1a_compute<64, preset>("123456789"sv) == fnv1a_compute<64>("123456789"sv));
  static_assert(engine_checks::split_check_text(fnv1a_engine<64, preset>{}) == fnv1a_compute<64>("123456789"sv));
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((fnv1a_compute<32, preset>(at(buffer, 3, 1000))), fnv1a_compute<32>(at(buffer, 3, 1000)));
}

TYPED_TEST(fnv1a_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(1024 + 8, 1);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer, 1024); });
}

TYPED_TEST(fnv1a_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

} // namespace
