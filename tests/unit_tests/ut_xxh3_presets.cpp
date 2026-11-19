// XXH3 presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/xxh3.hpp>
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
using namespace std::literals;

constexpr std::uint64_t test_seed = 0x9E3779B97F4A7C15U;

std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

template <unsigned Width, class Preset> void check_constant() {
  constexpr isa preset = Preset::value;
  static_assert((xxh3_compute<Width, preset>("abc"sv)) == (xxh3_compute<Width, isa::generic>("abc"sv)));
  static_assert((xxh3_compute<Width, preset>("123456789"sv, test_seed)) == (xxh3_compute<Width, isa::generic>("123456789"sv, test_seed)));
  static_assert(xxh3_finalize<Width, preset>(xxh3_update<Width, preset>(xxh3_update<Width, preset>(xxh3_state<Width>{.seed = test_seed}, "1234"sv),
                                                                       "56789"sv)) ==
                xxh3_finalize<Width, isa::generic>(
                    xxh3_update<Width, isa::generic>(xxh3_update<Width, isa::generic>(xxh3_state<Width>{.seed = test_seed}, "1234"sv), "56789"sv)));
  EXPECT_EQ((xxh3_compute<Width, preset>("abc"sv)), (xxh3_compute<Width, isa::generic>("abc"sv)));
  EXPECT_EQ((xxh3_compute<Width, preset>("123456789"sv, test_seed)), (xxh3_compute<Width, isa::generic>("123456789"sv, test_seed)));
}

template <unsigned Width, class Preset> void check_sizes(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      auto const data = at(buffer, offset, size);
      ASSERT_EQ((xxh3_compute<Width, preset>(data, test_seed)), (xxh3_compute<Width, isa::generic>(data, test_seed)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <unsigned Width, class Preset> void check_large(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const data = at(buffer, offset, size);
      ASSERT_EQ((xxh3_compute<Width, preset>(data, test_seed)), (xxh3_compute<Width, isa::generic>(data, test_seed)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <unsigned Width, class Preset> void check_updates(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const split : {1UZ, 63UZ, 64UZ, 65UZ, 127UZ, 128UZ, 129UZ, 240UZ, 241UZ, 255UZ, 256UZ, 257UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      auto const expected = xxh3_finalize<Width, isa::generic>(
          xxh3_update<Width, isa::generic>(xxh3_update<Width, isa::generic>(xxh3_state<Width>{.seed = test_seed}, head), tail));
      auto actual = xxh3_update<Width, preset>(xxh3_state<Width>{.seed = test_seed}, head.first(split / 2));
      actual = xxh3_update<Width, preset>(actual, head.subspan(split / 2));
      actual = xxh3_update<Width, preset>(actual, tail);
      ASSERT_EQ((xxh3_finalize<Width, preset>(actual)), expected) << "split " << split << ", offset " << offset;
    }
  }
  auto const value = (xxh3_compute<Width, isa::generic>(buffer, test_seed));
  EXPECT_EQ((xxh3_compute<Width, preset>(buffer, test_seed)), value);
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ((xxh3_compute<Width, preset>(list, test_seed)), (xxh3_compute<Width, isa::generic>(list, test_seed)));
  if constexpr(Width == 64) {
    EXPECT_EQ(xxh3_64_compute<preset>(buffer, test_seed), xxh3_64_compute(buffer, test_seed));
  } else {
    EXPECT_EQ(xxh3_128_compute<preset>(buffer, test_seed), xxh3_128_compute(buffer, test_seed));
  }
}

template <class Preset> using xxh3_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(xxh3_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(xxh3_presets, constant_evaluation) {
  check_constant<64, TypeParam>();
  check_constant<128, TypeParam>();
}

TYPED_TEST(xxh3_presets, match_generic_for_every_size_to_2048) {
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  check_sizes<64, TypeParam>(buffer);
  check_sizes<128, TypeParam>(buffer);
}

TYPED_TEST(xxh3_presets, match_generic_for_large_sizes) {
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  check_large<64, TypeParam>(buffer);
  check_large<128, TypeParam>(buffer);
}

TYPED_TEST(xxh3_presets, match_generic_through_updates_and_splits) {
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  check_updates<64, TypeParam>(buffer);
  check_updates<128, TypeParam>(buffer);
}

} // namespace
