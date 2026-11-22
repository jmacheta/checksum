// Adler-32 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/adler32.hpp>
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

template <class Preset> using adler32_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(adler32_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(adler32_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(adler32_compute<preset>("123456789"sv) == 0x091E01DE);
  static_assert(engine_checks::continued(adler32_engine<preset>{}, adler32_compute("12345678"sv), "9"sv) == 0x091E01DE);
  EXPECT_EQ(adler32_compute<preset>("123456789"sv), 0x091E01DEU);
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ(adler32_compute<preset>(at(buffer, 3, 1000)), adler32_compute(at(buffer, 3, 1000)));
}

TYPED_TEST(adler32_presets, match_generic_for_every_size) {
  engine_checks::check_every_size(adler32_engine<TypeParam::value>{}, adler32_engine{}, random_bytes(2048 + 8, 1));
}

TYPED_TEST(adler32_presets, match_generic_for_large_sizes) {
  engine_checks::check_large_sizes(adler32_engine<TypeParam::value>{}, adler32_engine{}, random_bytes(1'000'008, 2));
}

TYPED_TEST(adler32_presets, match_generic_through_updates_and_splits) {
  engine_checks::check_updates_and_splits(adler32_engine<TypeParam::value>{}, adler32_engine{}, random_bytes(3000, 3));
}

// From the initial and the largest sums: the inputs closest to overflowing the deferred sums.
TYPED_TEST(adler32_presets, match_generic_for_all_ones) {
  std::vector<std::byte> const ones((512UZ << 10U) + 64, std::byte{0xFF});
  engine_checks::check_all_ones(adler32_engine<TypeParam::value>{}, adler32_engine{}, ones, adler32_state{.sum1 = 0xFFFF, .sum2 = 0xFFFF});
}

} // namespace
