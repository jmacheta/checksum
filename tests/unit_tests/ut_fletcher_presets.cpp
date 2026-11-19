// Fletcher and Adler presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/adler32.hpp>
#include <checksum/fletcher.hpp>
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

// The engine of each preset, the check value of "123456789" and the largest sums.
template <unsigned Width, std::uint64_t Check>
  requires(Width == 16 || Width == 32 || Width == 64)
struct Fletcher {
  template <isa Isa = isa::generic> using engine = fletcher_engine<Width, Isa>;
  using state_type = fletcher_state<Width>;
  static constexpr state_type::value_type check = static_cast<state_type::value_type>(Check);
  static constexpr state_type largest_state{.sum1 = static_cast<state_type::sum_type>(-1), .sum2 = static_cast<state_type::sum_type>(-1)};
};

using Fletcher16 = Fletcher<16, 0x1EDE>;
using Fletcher32 = Fletcher<32, 0xDF09D509>;
using Fletcher64 = Fletcher<64, 0x0D0803376C6A689F>;

struct Adler32 {
  template <isa Isa = isa::generic> using engine = adler32_engine<Isa>;
  static constexpr std::uint32_t check = 0x091E01DE;
  static constexpr adler32_state largest_state{.sum1 = 0xFFFF, .sum2 = 0xFFFF};
};

template <class Algorithm, class Preset> void check_constant() {
  constexpr typename Algorithm::template engine<Preset::value> engine;
  static_assert(engine.compute(std::string_view("123456789")) == Algorithm::check);
  static_assert(engine.finalize(engine.update(engine.resume(engine.compute(std::string_view("12345678"))), std::string_view("9"))) ==
                Algorithm::check);
  EXPECT_EQ(engine.compute(std::string_view("123456789")), Algorithm::check);
}

template <class Algorithm, class Preset> void check_sizes(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine;
  constexpr typename Algorithm::template engine<> generic;
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_large(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine;
  constexpr typename Algorithm::template engine<> generic;
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_updates(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine;
  constexpr typename Algorithm::template engine<> generic;
  for(std::size_t const split : {1UZ, 63UZ, 191UZ, 192UZ, 193UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      auto const expected = generic.update(generic.update(generic.initial(), head), tail);
      auto actual = engine.update(engine.initial(), head.first(split / 2));
      actual = engine.update(actual, head.subspan(split / 2));
      actual = engine.update(actual, tail);
      ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
    }
  }
  auto const value = generic.compute(buffer);
  EXPECT_EQ(engine.finalize(engine.update(engine.resume(value), std::span<std::byte const>{})), value);
  EXPECT_EQ(engine.update(engine.resume(value), buffer), generic.update(generic.resume(value), buffer));
  EXPECT_EQ(engine.compute(buffer), generic.compute(buffer));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(engine.compute(list), generic.compute(list));
}

// All-ones bytes around the chunk sizes of every kernel, from the initial and the largest sums: the inputs that come closest to
// overflowing a kernel's deferred sums.
template <class Algorithm, class Preset> void check_overflow_bounds(std::vector<std::byte> const &ones) {
  constexpr typename Algorithm::template engine<Preset::value> engine;
  constexpr typename Algorithm::template engine<> generic;
  for(std::size_t const chunk : {1024UZ, 2048UZ, 4096UZ, 5600UZ, 5760UZ, 16UZ << 10U, 32UZ << 10U, 256UZ << 10U}) {
    for(std::size_t const size : {chunk - 1, chunk, chunk + 1, chunk + 35, (2 * chunk) + 35}) {
      for(std::size_t const offset : {0UZ, 1UZ, 3UZ}) {
        auto const message = at(ones, offset, size);
        ASSERT_EQ(engine.compute(message), generic.compute(message)) << "size " << size << ", offset " << offset;
        ASSERT_EQ(engine.update(Algorithm::largest_state, message), generic.update(Algorithm::largest_state, message))
            << "size " << size << ", offset " << offset;
      }
    }
  }
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

TYPED_TEST(fletcher_adler_presets, match_generic_at_overflow_bounds) {
  std::vector<std::byte> const ones((512UZ << 10U) + 64, std::byte{0xFF});
  check_overflow_bounds<Fletcher16, TypeParam>(ones);
  check_overflow_bounds<Fletcher32, TypeParam>(ones);
  check_overflow_bounds<Fletcher64, TypeParam>(ones);
  check_overflow_bounds<Adler32, TypeParam>(ones);
}

} // namespace
