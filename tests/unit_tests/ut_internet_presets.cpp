// Internet checksum presets: every preset this build has must give bit-identical results to isa::generic, at run time and
// at compile time.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include <engine_checks.hpp>
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

template <class Preset> using internet_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(internet_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(internet_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  constexpr internet_engine<preset> internet;
  static_assert(internet_compute<preset>("123456789"sv) == 0xF62A);
  static_assert(engine_checks::continued(internet, internet_compute("12345678"sv), "9"sv) == 0xF62A);
  EXPECT_EQ(internet_compute<preset>("123456789"sv), 0xF62A);
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ(internet_compute<preset>(at(buffer, 3, 1000)), internet_compute(at(buffer, 3, 1000)));
}

TYPED_TEST(internet_presets, match_generic_for_every_size) {
  engine_checks::check_every_size(internet_engine<TypeParam::value>{}, internet_engine{}, random_bytes(2048 + 8, 1));
}

TYPED_TEST(internet_presets, match_generic_for_large_sizes) {
  engine_checks::check_large_sizes(internet_engine<TypeParam::value>{}, internet_engine{}, random_bytes(1'000'008, 2));
}

TYPED_TEST(internet_presets, match_generic_through_updates_and_splits) {
  engine_checks::check_updates_and_splits(internet_engine<TypeParam::value>{}, internet_engine{}, random_bytes(3000, 3));
}

// All-ones bytes make every word the kernels add carry out.
TYPED_TEST(internet_presets, match_generic_for_all_ones) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const ones((1 << 20) + 8, std::byte{0xFF});
  engine_checks::check_all_ones(internet_engine<preset>{}, internet_engine{}, ones, internet_state{.sum = 0xFFFF, .odd = true});
  for(std::size_t offset = 0; offset < 4; ++offset) {
    for(std::size_t size = 0; size <= 600; ++size) {
      ASSERT_EQ(internet_compute<preset>(at(ones, offset, size)), internet_compute(at(ones, offset, size)))
          << "offset " << offset << ", size " << size;
    }
    EXPECT_EQ(internet_compute<preset>(at(ones, offset, (1 << 20) + 1)), 0x00FF) << "offset " << offset;
  }
}

} // namespace
