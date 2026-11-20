// MurmurHash3 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/isa.hpp>
#include <checksum/murmur3.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstdint>
#include <string_view>

namespace {

using namespace checksum;
using namespace std::literals;

constexpr std::uint32_t murmur3_seed = 0x9747B28CU;

// Calls check(murmur3_engine<Width, Preset>(murmur3_seed), murmur3_engine<Width>(murmur3_seed)) for every width.
template <isa Preset, class Check> void for_each_width(Check check) {
  check(murmur3_engine<32, Preset>(murmur3_seed), murmur3_engine<32>(murmur3_seed));
  check(murmur3_engine<128, Preset>(murmur3_seed), murmur3_engine<128>(murmur3_seed));
}

template <class Preset> using murmur3_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(murmur3_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(murmur3_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(murmur3_compute<32, preset>("123456789"sv, murmur3_seed) == murmur3_compute<32>("123456789"sv, murmur3_seed));
  static_assert(murmur3_compute<128, preset>("123456789"sv, murmur3_seed) == murmur3_compute<128>("123456789"sv, murmur3_seed));
  static_assert(engine_checks::split_check_text(murmur3_engine<32, preset>(murmur3_seed)) == murmur3_compute<32>("123456789"sv, murmur3_seed));
  static_assert(engine_checks::split_check_text(murmur3_engine<128, preset>(murmur3_seed)) == murmur3_compute<128>("123456789"sv, murmur3_seed));
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((murmur3_compute<32, preset>(at(buffer, 3, 1000), murmur3_seed)), murmur3_compute<32>(at(buffer, 3, 1000), murmur3_seed));
  EXPECT_EQ((murmur3_compute<128, preset>(at(buffer, 3, 1000), murmur3_seed)), murmur3_compute<128>(at(buffer, 3, 1000), murmur3_seed));
}

// Up to 1024 bytes: past the 16- and 32-byte blocks many times over.
TYPED_TEST(murmur3_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(1024 + 8, 1);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer, 1024); });
}

TYPED_TEST(murmur3_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_large_sizes(engine, generic, buffer); });
}

TYPED_TEST(murmur3_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

} // namespace
