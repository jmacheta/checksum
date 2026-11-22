// XXH3 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/isa.hpp>
#include <checksum/xxh3.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstdint>
#include <string_view>

namespace {

using namespace checksum;
using namespace std::literals;

constexpr std::uint64_t xxh3_seed = 0x9E3779B97F4A7C15U;

// Calls check(xxh3_engine<Width, Preset>(xxh3_seed), xxh3_engine<Width>(xxh3_seed)) for every width.
template <isa Preset, class Check> void for_each_width(Check check) {
  check(xxh3_engine<64, Preset>(xxh3_seed), xxh3_engine<64>(xxh3_seed));
  check(xxh3_engine<128, Preset>(xxh3_seed), xxh3_engine<128>(xxh3_seed));
}

template <unsigned Width, isa Preset> void check_constant() {
  static_assert((xxh3_compute<Width, Preset>("abc"sv)) == (xxh3_compute<Width>("abc"sv)));
  static_assert((xxh3_compute<Width, Preset>("123456789"sv, xxh3_seed)) == (xxh3_compute<Width>("123456789"sv, xxh3_seed)));
  constexpr xxh3_engine<Width, Preset> engine(xxh3_seed);
  constexpr xxh3_engine<Width> generic(xxh3_seed);
  static_assert(engine.finalize(engine.update(engine.update(engine.initial(), "1234"sv), "56789"sv)) == generic.compute("123456789"sv));
  EXPECT_EQ((xxh3_compute<Width, Preset>("abc"sv)), (xxh3_compute<Width>("abc"sv)));
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((xxh3_compute<Width, Preset>(at(buffer, 3, 1000), xxh3_seed)), (xxh3_compute<Width>(at(buffer, 3, 1000), xxh3_seed)));
}

template <class Preset> using xxh3_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(xxh3_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(xxh3_presets, constant_evaluation) {
  check_constant<64, TypeParam::value>();
  check_constant<128, TypeParam::value>();
}

TYPED_TEST(xxh3_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(2048 + 8, 1);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer); });
}

TYPED_TEST(xxh3_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_large_sizes(engine, generic, buffer); });
}

TYPED_TEST(xxh3_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

} // namespace
