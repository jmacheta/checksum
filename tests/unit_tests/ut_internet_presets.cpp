// Internet checksum presets: every preset this build has must give bit-identical results to isa::generic, at run time and
// at compile time.

#include <checksum/internet.hpp>
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

// The size bytes of buffer from offset: an unaligned start address for most offsets.
std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

template <class Preset> using internet_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(internet_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code for every preset.
TYPED_TEST(internet_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  static_assert(internet_compute<preset>(std::string_view("123456789")) == 0xF62A);
  static_assert(internet_update<preset>(internet_compute(std::string_view("12345678")), std::string_view("9")) == 0xF62A);
  EXPECT_EQ(internet_compute<preset>(std::string_view("123456789")), 0xF62A);
}

// All lengths past every kernel threshold (64 to 512 bytes) and the 64-byte blocks after it, at every start modulo 8.
TYPED_TEST(internet_presets, match_generic_for_every_size_to_2048) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      ASSERT_EQ(internet_compute<preset>(at(buffer, offset, size)), internet_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

TYPED_TEST(internet_presets, match_generic_for_large_sizes) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(internet_compute<preset>(at(buffer, offset, size)), internet_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

// All-ones bytes make every word the kernels add carry out.
TYPED_TEST(internet_presets, match_generic_with_carries) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const ones((1 << 20) + 8, std::byte{0xFF});
  for(std::size_t offset = 0; offset < 4; ++offset) {
    for(std::size_t size = 0; size <= 600; ++size) {
      ASSERT_EQ(internet_compute<preset>(at(ones, offset, size)), internet_compute(at(ones, offset, size)))
          << "offset " << offset << ", size " << size;
    }
    EXPECT_EQ(internet_compute<preset>(at(ones, offset, (1 << 20) + 1)), 0x00FF) << "offset " << offset;
  }
}

TYPED_TEST(internet_presets, match_generic_through_updates_and_splits) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  // One state through three updates, split anywhere, including inside words and around the kernel thresholds, at
  // several start offsets.
  for(std::size_t const split : {1UZ, 63UZ, 191UZ, 192UZ, 193UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      internet_state const expected = internet_update(internet_update(internet_state{}, head), tail);
      internet_state actual = internet_update<preset>(internet_state{}, head.first(split / 2));
      actual = internet_update<preset>(actual, head.subspan(split / 2));
      actual = internet_update<preset>(actual, tail);
      ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
    }
  }
  // The checksum overloads and byte ranges instead of spans.
  std::uint16_t const value = internet_compute(buffer);
  EXPECT_EQ(internet_update<preset>(value, std::span<std::byte const>{}), value);
  EXPECT_EQ(internet_update<preset>(value, buffer), internet_update(value, buffer));
  EXPECT_EQ(internet_compute<preset>(buffer), internet_compute(buffer));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(internet_compute<preset>(list), internet_compute(buffer));
}

} // namespace
