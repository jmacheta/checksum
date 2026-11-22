// SHA presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/isa.hpp>
#include <checksum/sha.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <string_view>

namespace {

using namespace checksum;
using namespace std::literals;
using enum sha_variant;

// Calls check(sha_engine<Variant, Strategy, Preset>{}, sha_engine<Variant>{}) for one variant of each compression function and
// both strategies.
template <isa Preset, class Check> void for_each_core(Check check) {
  check(sha_engine<sha1, sha_compact, Preset>{}, sha_engine<sha1>{});
  check(sha_engine<sha256, sha_compact, Preset>{}, sha_engine<sha256>{});
  check(sha_engine<sha512, sha_compact, Preset>{}, sha_engine<sha512>{});
  check(sha_engine<sha1, sha_unrolled, Preset>{}, sha_engine<sha1>{});
  check(sha_engine<sha256, sha_unrolled, Preset>{}, sha_engine<sha256>{});
  check(sha_engine<sha512, sha_unrolled, Preset>{}, sha_engine<sha512>{});
}

template <class Preset> using sha_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(sha_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(sha_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(sha_compute<sha256, sha_compact, preset>("123456789"sv) == sha_compute<sha256>("123456789"sv));
  static_assert(engine_checks::split_check_text(sha_engine<sha512, sha_unrolled, preset>{}) == sha_compute<sha512>("123456789"sv));
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((sha_compute<sha1, sha_compact, preset>(at(buffer, 3, 1000))), sha_compute<sha1>(at(buffer, 3, 1000)));
}

TYPED_TEST(sha_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(600 + 8, 1);
  for_each_core<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer, 600); });
}

TYPED_TEST(sha_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  for_each_core<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_large_sizes(engine, generic, buffer); });
}

TYPED_TEST(sha_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_core<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

} // namespace
