// MurmurHash3 and xxHash presets: every preset this build has must give bit-identical results to isa::generic, at run time and at compile time.

#include <checksum/isa.hpp>
#include <checksum/murmur3.hpp>
#include <checksum/xxhash.hpp>

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

constexpr std::uint32_t murmur3_seed = 0x9747B28CU;
constexpr std::uint32_t xxh32_seed = 0x9E3779B1U;
constexpr std::uint64_t xxh64_seed = 0x9E3779B185EBCA87ULL;

std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

template <unsigned Width, std::uint32_t Seed>
  requires(Width == 32 || Width == 128)
struct Murmur3 {
  using state_type = murmur3_state<Width>;
  using checksum_type = state_type::value_type;
  using seed_type = std::uint32_t;
  static constexpr seed_type seed = Seed;
  static constexpr state_type initial_state() noexcept { return {.seed = seed}; }
  static constexpr checksum_type finalize(state_type const &state) noexcept { return murmur3_finalize(state); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type compute(Range &&data) noexcept {
    return murmur3_compute<Width, Isa>(std::forward<Range>(data), seed);
  }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr state_type update(state_type state, Range &&data) noexcept {
    return murmur3_update<Width, Isa>(state, std::forward<Range>(data));
  }
};

template <unsigned Width, std::uint64_t Seed>
  requires(Width == 32 || Width == 64)
struct Xxhash {
  using state_type = xxhash_state<Width>;
  using checksum_type = state_type::value_type;
  using seed_type = checksum_type;
  static constexpr seed_type seed = static_cast<seed_type>(Seed);
  static constexpr state_type initial_state() noexcept { return {.seed = seed}; }
  static constexpr checksum_type finalize(state_type const &state) noexcept { return xxhash_finalize(state); }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr checksum_type compute(Range &&data) noexcept {
    return xxhash_compute<Width, Isa>(std::forward<Range>(data), seed);
  }
  template <isa Isa = isa::generic, byte_range Range>
  static constexpr state_type update(state_type state, Range &&data) noexcept {
    return xxhash_update<Width, Isa>(state, std::forward<Range>(data));
  }
};

using Murmur3_32 = Murmur3<32, murmur3_seed>;
using Murmur3_128 = Murmur3<128, murmur3_seed>;
using Xxh32 = Xxhash<32, xxh32_seed>;
using Xxh64 = Xxhash<64, xxh64_seed>;

template <class Algorithm, class Preset> void check_constant() {
  constexpr isa preset = Preset::value;
  static_assert(Algorithm::template compute<preset>(std::string_view("123456789")) == Algorithm::compute(std::string_view("123456789")));
  static_assert(Algorithm::finalize(Algorithm::template update<preset>(Algorithm::template update<preset>(Algorithm::initial_state(),
                                                                                                        std::string_view("12345678")),
                                                                      std::string_view("9"))) ==
                Algorithm::finalize(Algorithm::update(Algorithm::update(Algorithm::initial_state(), std::string_view("12345678")),
                                                      std::string_view("9"))));
  EXPECT_EQ(Algorithm::template compute<preset>(std::string_view("123456789")), Algorithm::compute(std::string_view("123456789")));
}

template <class Algorithm, class Preset> void check_sizes(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 1024; ++size) {
      ASSERT_EQ(Algorithm::template compute<preset>(at(buffer, offset, size)), Algorithm::compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_large(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const size : {1023UZ, 1024UZ, 1025UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(Algorithm::template compute<preset>(at(buffer, offset, size)), Algorithm::compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_updates(std::vector<std::byte> const &buffer) {
  constexpr isa preset = Preset::value;
  for(std::size_t const split : {1UZ,  3UZ,  4UZ,  5UZ,  7UZ,  8UZ,  9UZ,  15UZ,  16UZ,  17UZ,  31UZ,  32UZ,  33UZ,
                                 63UZ, 64UZ, 65UZ, 127UZ, 128UZ, 129UZ, 255UZ, 256UZ, 257UZ, 511UZ, 512UZ, 513UZ,
                                 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      auto const expected = Algorithm::update(Algorithm::update(Algorithm::initial_state(), head), tail);
      auto actual = Algorithm::template update<preset>(Algorithm::initial_state(), head.first(split / 2));
      actual = Algorithm::template update<preset>(actual, head.subspan(split / 2));
      actual = Algorithm::template update<preset>(actual, tail);
      ASSERT_EQ(Algorithm::finalize(actual), Algorithm::finalize(expected)) << "split " << split << ", offset " << offset;
    }
  }
  auto const value = Algorithm::compute(buffer);
  EXPECT_EQ(Algorithm::template compute<preset>(buffer), value);
  EXPECT_EQ(Algorithm::finalize(Algorithm::template update<preset>(Algorithm::initial_state(), std::span<std::byte const>{})),
            Algorithm::finalize(Algorithm::initial_state()));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(Algorithm::template compute<preset>(list), Algorithm::compute(list));
}

template <class Preset> using hash_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(hash_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(hash_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  check_constant<Murmur3_32, TypeParam>();
  check_constant<Murmur3_128, TypeParam>();
  check_constant<Xxh32, TypeParam>();
  check_constant<Xxh64, TypeParam>();
  static_assert(murmur3_32_compute<preset>(std::string_view("123456789"), murmur3_seed) ==
                murmur3_32_compute(std::string_view("123456789"), murmur3_seed));
  static_assert(murmur3_128_compute<preset>(std::string_view("123456789"), murmur3_seed) ==
                murmur3_128_compute(std::string_view("123456789"), murmur3_seed));
  static_assert(xxh32_compute<preset>(std::string_view("123456789"), xxh32_seed) ==
                xxh32_compute(std::string_view("123456789"), xxh32_seed));
  static_assert(xxh64_compute<preset>(std::string_view("123456789"), xxh64_seed) ==
                xxh64_compute(std::string_view("123456789"), xxh64_seed));
}

TYPED_TEST(hash_presets, match_generic_for_every_size_to_1024) {
  std::vector<std::byte> const buffer = random_bytes(1024 + 8, 1);
  check_sizes<Murmur3_32, TypeParam>(buffer);
  check_sizes<Murmur3_128, TypeParam>(buffer);
  check_sizes<Xxh32, TypeParam>(buffer);
  check_sizes<Xxh64, TypeParam>(buffer);
}

TYPED_TEST(hash_presets, match_generic_for_large_sizes) {
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  check_large<Murmur3_32, TypeParam>(buffer);
  check_large<Murmur3_128, TypeParam>(buffer);
  check_large<Xxh32, TypeParam>(buffer);
  check_large<Xxh64, TypeParam>(buffer);
}

TYPED_TEST(hash_presets, match_generic_through_updates_and_splits) {
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  check_updates<Murmur3_32, TypeParam>(buffer);
  check_updates<Murmur3_128, TypeParam>(buffer);
  check_updates<Xxh32, TypeParam>(buffer);
  check_updates<Xxh64, TypeParam>(buffer);
}

TYPED_TEST(hash_presets, aliases_match_generic) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(1024 + 8, 4);
  auto const data = at(buffer, 3, 1000);
  EXPECT_EQ(murmur3_32_compute<preset>(data, murmur3_seed), murmur3_32_compute(data, murmur3_seed));
  EXPECT_EQ(murmur3_128_compute<preset>(data, murmur3_seed), murmur3_128_compute(data, murmur3_seed));
  EXPECT_EQ(xxh32_compute<preset>(data, xxh32_seed), xxh32_compute(data, xxh32_seed));
  EXPECT_EQ(xxh64_compute<preset>(data, xxh64_seed), xxh64_compute(data, xxh64_seed));
}

} // namespace
