// fletcher4 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;

std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

template <class Preset> using fletcher4_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(fletcher4_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(fletcher4_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(fletcher4_compute<preset>(std::string_view("abcdefgh")) ==
                fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
  static_assert(fletcher4_finalize(fletcher4_update<preset>(
                    fletcher4_update<preset>(fletcher4_state{}, std::string_view("abcdefg")), std::string_view("h"))) ==
                fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
  EXPECT_EQ(fletcher4_compute<preset>(std::string_view("abcdefgh")),
            (fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9}));
}

TYPED_TEST(fletcher4_presets, match_generic_for_every_size_to_2048) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      ASSERT_EQ(fletcher4_compute<preset>(at(buffer, offset, size)), fletcher4_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

TYPED_TEST(fletcher4_presets, match_generic_for_large_sizes) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(fletcher4_compute<preset>(at(buffer, offset, size)), fletcher4_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

TYPED_TEST(fletcher4_presets, match_generic_through_updates_and_splits) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  for(std::size_t const split : {1UZ, 63UZ, 191UZ, 192UZ, 193UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      fletcher4_state const expected = fletcher4_update(fletcher4_update(fletcher4_state{}, head), tail);
      fletcher4_state actual = fletcher4_update<preset>(fletcher4_state{}, head.first(split / 2));
      actual = fletcher4_update<preset>(actual, head.subspan(split / 2));
      actual = fletcher4_update<preset>(actual, tail);
      ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
    }
  }
  EXPECT_EQ(fletcher4_update<preset>(fletcher4_state{}, std::span<std::byte const>{}), fletcher4_state{});
  EXPECT_EQ(fletcher4_compute<preset>(buffer), fletcher4_compute(buffer));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(fletcher4_compute<preset>(list), fletcher4_compute(list));
}

} // namespace
