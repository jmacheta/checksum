// fletcher4 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>

#include <engine_checks.hpp>
#include <fletcher4_reference_vectors.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

constexpr fletcher4_value abcdefgh{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9};

template <class Preset> using fletcher4_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(fletcher4_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(fletcher4_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(fletcher4_compute<preset>("abcdefgh"sv) == abcdefgh);
  static_assert(engine_checks::continued(fletcher4_engine<preset>{}, fletcher4_compute("abcd"sv), "abcdefgh"sv.substr(4)) == abcdefgh);
  static_assert(fletcher4_engine{}.finalize(fletcher4_engine<preset>{}.update(fletcher4_engine<preset>{}.update({}, "abcdefg"sv), "h"sv)) ==
                abcdefgh);
  EXPECT_EQ(fletcher4_compute<preset>("abcdefgh"sv), abcdefgh);
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ(fletcher4_compute<preset>(at(buffer, 3, 1000)), fletcher4_compute(at(buffer, 3, 1000)));
}

TYPED_TEST(fletcher4_presets, match_generic_for_every_size) {
  engine_checks::check_every_size(fletcher4_engine<TypeParam::value>{}, fletcher4_engine{}, random_bytes(2048 + 8, 1));
}

TYPED_TEST(fletcher4_presets, match_generic_for_large_sizes) {
  engine_checks::check_large_sizes(fletcher4_engine<TypeParam::value>{}, fletcher4_engine{}, random_bytes(1'000'008, 2));
}

TYPED_TEST(fletcher4_presets, match_generic_through_updates_and_splits) {
  engine_checks::check_updates_and_splits(fletcher4_engine<TypeParam::value>{}, fletcher4_engine{}, random_bytes(3000, 3));
}

// All-ones words wrap the 64-bit sums many times over: the known value, odd tails after it, and runs from sums that wrap.
TYPED_TEST(fletcher4_presets, match_generic_for_all_ones) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const ones(fletcher4_test::all_ones_size + 3, std::byte{0xFF});
  EXPECT_EQ(fletcher4_compute<preset>(at(ones, 0, fletcher4_test::all_ones_size)), fletcher4_test::all_ones);
  for(std::size_t const size : {fletcher4_test::all_ones_size + 1, fletcher4_test::all_ones_size + 3}) {
    ASSERT_EQ(fletcher4_compute<preset>(at(ones, 0, size)), fletcher4_compute(at(ones, 0, size))) << "size " << size;
  }
  constexpr std::uint64_t largest = ~std::uint64_t{0};
  engine_checks::check_all_ones(fletcher4_engine<preset>{}, fletcher4_engine{}, ones,
                                fletcher4_state{.sum1 = largest, .sum2 = largest, .sum3 = largest, .sum4 = largest});
}

} // namespace
