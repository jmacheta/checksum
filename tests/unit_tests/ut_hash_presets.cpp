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
#include <vector>

namespace {

using namespace checksum;

constexpr std::uint32_t murmur3_seed = 0x9747B28CU;
constexpr std::uint32_t xxh32_seed = 0x9E3779B1U;
constexpr std::uint64_t xxh64_seed = 0x9E3779B185EBCA87ULL;

std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

// The engine of each preset and the seed of the tests.
template <unsigned Width, std::uint32_t Seed>
  requires(Width == 32 || Width == 128)
struct Murmur3 {
  template <isa Isa = isa::generic> using engine = murmur3_engine<Width, Isa>;
  static constexpr std::uint32_t seed = Seed;
};

template <unsigned Width, std::uint64_t Seed>
  requires(Width == 32 || Width == 64)
struct Xxhash {
  template <isa Isa = isa::generic> using engine = xxhash_engine<Width, Isa>;
  static constexpr auto seed = static_cast<xxhash_state<Width>::value_type>(Seed);
};

using Murmur3_32 = Murmur3<32, murmur3_seed>;
using Murmur3_128 = Murmur3<128, murmur3_seed>;
using Xxh32 = Xxhash<32, xxh32_seed>;
using Xxh64 = Xxhash<64, xxh64_seed>;

template <class Algorithm, class Preset> void check_constant() {
  constexpr typename Algorithm::template engine<Preset::value> engine(Algorithm::seed);
  constexpr typename Algorithm::template engine<> generic(Algorithm::seed);
  static_assert(engine.compute(std::string_view("123456789")) == generic.compute(std::string_view("123456789")));
  static_assert(engine.finalize(engine.update(engine.update(engine.initial(), std::string_view("12345678")), std::string_view("9"))) ==
                generic.finalize(generic.update(generic.update(generic.initial(), std::string_view("12345678")), std::string_view("9"))));
  EXPECT_EQ(engine.compute(std::string_view("123456789")), generic.compute(std::string_view("123456789")));
}

template <class Algorithm, class Preset> void check_sizes(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine(Algorithm::seed);
  constexpr typename Algorithm::template engine<> generic(Algorithm::seed);
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 1024; ++size) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_large(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine(Algorithm::seed);
  constexpr typename Algorithm::template engine<> generic(Algorithm::seed);
  for(std::size_t const size : {1023UZ, 1024UZ, 1025UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <class Algorithm, class Preset> void check_updates(std::vector<std::byte> const &buffer) {
  constexpr typename Algorithm::template engine<Preset::value> engine(Algorithm::seed);
  constexpr typename Algorithm::template engine<> generic(Algorithm::seed);
  for(std::size_t const split : {1UZ,  3UZ,  4UZ,   5UZ,   7UZ,   8UZ,   9UZ,   15UZ,  16UZ,  17UZ,  31UZ,  32UZ,   33UZ,  63UZ,
                                 64UZ, 65UZ, 127UZ, 128UZ, 129UZ, 255UZ, 256UZ, 257UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      auto const expected = generic.update(generic.update(generic.initial(), head), tail);
      auto actual = engine.update(engine.initial(), head.first(split / 2));
      actual = engine.update(actual, head.subspan(split / 2));
      actual = engine.update(actual, tail);
      ASSERT_EQ(engine.finalize(actual), generic.finalize(expected)) << "split " << split << ", offset " << offset;
    }
  }
  auto const value = generic.compute(buffer);
  EXPECT_EQ(engine.compute(buffer), value);
  EXPECT_EQ(engine.finalize(engine.update(engine.initial(), std::span<std::byte const>{})), generic.finalize(generic.initial()));
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(engine.compute(list), generic.compute(list));
}

template <class Preset> using hash_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(hash_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(hash_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  check_constant<Murmur3_32, TypeParam>();
  check_constant<Murmur3_128, TypeParam>();
  check_constant<Xxh32, TypeParam>();
  check_constant<Xxh64, TypeParam>();
  static_assert(murmur3_compute<32, preset>(std::string_view("123456789"), murmur3_seed) ==
                murmur3_compute<32>(std::string_view("123456789"), murmur3_seed));
  static_assert(murmur3_compute<128, preset>(std::string_view("123456789"), murmur3_seed) ==
                murmur3_compute<128>(std::string_view("123456789"), murmur3_seed));
  static_assert(xxhash_compute<32, preset>(std::string_view("123456789"), xxh32_seed) ==
                xxhash_compute<32>(std::string_view("123456789"), xxh32_seed));
  static_assert(xxhash_compute<64, preset>(std::string_view("123456789"), xxh64_seed) ==
                xxhash_compute<64>(std::string_view("123456789"), xxh64_seed));
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

TYPED_TEST(hash_presets, compute_matches_generic) {
  constexpr isa preset = TypeParam::value;
  std::vector<std::byte> const buffer = random_bytes(1024 + 8, 4);
  auto const data = at(buffer, 3, 1000);
  EXPECT_EQ((murmur3_compute<32, preset>(data, murmur3_seed)), murmur3_compute<32>(data, murmur3_seed));
  EXPECT_EQ((murmur3_compute<128, preset>(data, murmur3_seed)), murmur3_compute<128>(data, murmur3_seed));
  EXPECT_EQ((xxhash_compute<32, preset>(data, xxh32_seed)), xxhash_compute<32>(data, xxh32_seed));
  EXPECT_EQ((xxhash_compute<64, preset>(data, xxh64_seed)), xxhash_compute<64>(data, xxh64_seed));
}

} // namespace
