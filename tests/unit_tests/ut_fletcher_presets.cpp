// Fletcher and Adler presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/fletcher.hpp>
#include <checksum/adler32.hpp>
#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace checksum;

std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

template <unsigned Width, std::uint64_t Check>
  requires(Width == 16 || Width == 32 || Width == 64)
struct Fletcher {
  using state_type = fletcher_state<Width>;
  using checksum_type = state_type::value_type;
  static constexpr checksum_type check = static_cast<checksum_type>(Check);
  static constexpr state_type initial_state() noexcept { return {}; }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type compute(Range &&data) noexcept { return fletcher_compute<Width, Isa>(std::forward<Range>(data)); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr state_type update(state_type state, Range &&data) noexcept { return fletcher_update<Width, Isa>(state, std::forward<Range>(data)); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type update(checksum_type checksum, Range &&data) noexcept { return fletcher_update<Isa>(checksum, std::forward<Range>(data)); }
};

using Fletcher16 = Fletcher<16, 0x1EDE>;
using Fletcher32 = Fletcher<32, 0xDF09D509>;
using Fletcher64 = Fletcher<64, 0x0D0803376C6A689F>;

struct Adler32 {
  using state_type = adler32_state;
  using checksum_type = std::uint32_t;
  static constexpr checksum_type check = 0x091E01DE;
  static constexpr state_type initial_state() noexcept { return {}; }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type compute(Range &&data) noexcept { return adler32_compute<Isa>(std::forward<Range>(data)); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr state_type update(state_type state, Range &&data) noexcept { return adler32_update<Isa>(state, std::forward<Range>(data)); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type update(checksum_type checksum, Range &&data) noexcept { return adler32_update<Isa>(checksum, std::forward<Range>(data)); }
};

template <class Algorithm, class Preset> void check_constant() {
  constexpr isa preset = Preset::value;
  static_assert(Algorithm::template compute<preset>(std::string_view("123456789")) == Algorithm::check);
  static_assert(Algorithm::template update<preset>(Algorithm::template compute<preset>(std::string_view("12345678")), std::string_view("9")) == Algorithm::check);
  EXPECT_EQ(Algorithm::template compute<preset>(std::string_view("123456789")), Algorithm::check);
}

template <class Algorithm, class Preset> void check_sizes(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      ASSERT_EQ(Algorithm::template compute<preset>(at(buffer, offset, size)), Algorithm::compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_large(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(Algorithm::template compute<preset>(at(buffer, offset, size)), Algorithm::compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_updates(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const split : {1UZ, 63UZ, 191UZ, 192UZ, 193UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      auto const expected = Algorithm::update(Algorithm::update(Algorithm::initial_state(), head), tail);
      auto actual = Algorithm::template update<preset>(Algorithm::initial_state(), head.first(split / 2));
      actual = Algorithm::template update<preset>(actual, head.subspan(split / 2));
      actual = Algorithm::template update<preset>(actual, tail);
      ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
    }
  }
  auto const value = Algorithm::compute(buffer);
  EXPECT_EQ(Algorithm::template update<preset>(value, std::span<std::byte const>{}), value);
  EXPECT_EQ(Algorithm::template update<preset>(value, buffer), Algorithm::update(value, buffer));
  EXPECT_EQ(Algorithm::template compute<preset>(buffer), Algorithm::compute(buffer));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(Algorithm::template compute<preset>(list), Algorithm::compute(list));
}

template <class Preset> using fletcher_adler_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(fletcher_adler_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(fletcher_adler_presets, constant_evaluation) {
  check_constant<Fletcher16, TypeParam>();
  check_constant<Fletcher32, TypeParam>();
  check_constant<Fletcher64, TypeParam>();
  check_constant<Adler32, TypeParam>();
}

TYPED_TEST(fletcher_adler_presets, match_generic_for_every_size_to_2048) {
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  check_sizes<Fletcher16, TypeParam>(buffer);
  check_sizes<Fletcher32, TypeParam>(buffer);
  check_sizes<Fletcher64, TypeParam>(buffer);
  check_sizes<Adler32, TypeParam>(buffer);
}

TYPED_TEST(fletcher_adler_presets, match_generic_for_large_sizes) {
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  check_large<Fletcher16, TypeParam>(buffer);
  check_large<Fletcher32, TypeParam>(buffer);
  check_large<Fletcher64, TypeParam>(buffer);
  check_large<Adler32, TypeParam>(buffer);
}

TYPED_TEST(fletcher_adler_presets, match_generic_through_updates_and_splits) {
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  check_updates<Fletcher16, TypeParam>(buffer);
  check_updates<Fletcher32, TypeParam>(buffer);
  check_updates<Fletcher64, TypeParam>(buffer);
  check_updates<Adler32, TypeParam>(buffer);
}

} // namespace