// XXH32 and XXH64 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/isa.hpp>
#include <checksum/xxhash.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstdint>
#include <string_view>

namespace {

using namespace checksum;
using namespace std::literals;

constexpr std::uint32_t xxh32_seed = 0x9E3779B1U;
constexpr std::uint64_t xxh64_seed = 0x9E3779B185EBCA87ULL;

// Calls check(xxhash_engine<Width, Preset>(seed), xxhash_engine<Width>(seed)) for every width.
template <isa Preset, class Check> void for_each_width(Check check) {
  check(xxhash_engine<32, Preset>(xxh32_seed), xxhash_engine<32>(xxh32_seed));
  check(xxhash_engine<64, Preset>(xxh64_seed), xxhash_engine<64>(xxh64_seed));
}

template <class Preset> using xxhash_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(xxhash_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(xxhash_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(xxhash_compute<32, preset>("123456789"sv, xxh32_seed) == xxhash_compute<32>("123456789"sv, xxh32_seed));
  static_assert(xxhash_compute<64, preset>("123456789"sv, xxh64_seed) == xxhash_compute<64>("123456789"sv, xxh64_seed));
  static_assert(engine_checks::split_check_text(xxhash_engine<32, preset>(xxh32_seed)) == xxhash_compute<32>("123456789"sv, xxh32_seed));
  static_assert(engine_checks::split_check_text(xxhash_engine<64, preset>(xxh64_seed)) == xxhash_compute<64>("123456789"sv, xxh64_seed));
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((xxhash_compute<32, preset>(at(buffer, 3, 1000), xxh32_seed)), xxhash_compute<32>(at(buffer, 3, 1000), xxh32_seed));
  EXPECT_EQ((xxhash_compute<64, preset>(at(buffer, 3, 1000), xxh64_seed)), xxhash_compute<64>(at(buffer, 3, 1000), xxh64_seed));
}

// Up to 1024 bytes: past the 16- and 32-byte blocks many times over.
TYPED_TEST(xxhash_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(1024 + 8, 1);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer, 1024); });
}

TYPED_TEST(xxhash_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  for_each_width<TypeParam::value>([&](auto const &engine, auto const &generic) { engine_checks::check_large_sizes(engine, generic, buffer); });
}

TYPED_TEST(xxhash_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_width<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

} // namespace
