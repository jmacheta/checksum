// Internet checksum strategies: internet_loop_for<isa_x86_avx2> must give bit-identical results to internet_loop.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace checksum;

#ifdef CHECKSUM_INTERNET_X86_AVX2

using avx2 = internet_loop_for<isa_x86_avx2>;

// Constant evaluation runs the portable loop for every strategy, AVX2 included.
static_assert(internet_compute<avx2>(std::string_view("123456789")) == 0xF62A);
static_assert(internet_compute<avx2>(std::string_view("123456789")) == internet_compute(std::string_view("123456789")));

// The build compiled the AVX2 loop; the CPU running the test may still be too old to execute it.
bool cpu_has_avx2() {
#if defined(__x86_64__) && defined(__GNUC__)
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}

// The size bytes of buffer from offset: an unaligned start address for most offsets.
std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

TEST(internet_strategy, avx2_matches_default_for_every_size_to_2048) {
  if(!cpu_has_avx2()) {
    GTEST_SKIP() << "the CPU has no AVX2";
  }
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= 2048; ++size) {
      ASSERT_EQ(internet_compute<avx2>(at(buffer, offset, size)), internet_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

TEST(internet_strategy, avx2_matches_default_for_large_sizes) {
  if(!cpu_has_avx2()) {
    GTEST_SKIP() << "the CPU has no AVX2";
  }
  std::vector<std::byte> const buffer = random_bytes(1'000'008, 2);
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(internet_compute<avx2>(at(buffer, offset, size)), internet_compute(at(buffer, offset, size)))
          << "offset " << offset << ", size " << size;
    }
  }
}

TEST(internet_strategy, avx2_matches_default_through_updates_and_splits) {
  if(!cpu_has_avx2()) {
    GTEST_SKIP() << "the CPU has no AVX2";
  }
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  // One state through three updates, split anywhere, including inside words and around the kernel threshold, at
  // several start offsets.
  for(std::size_t const split : {1UZ, 63UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, 1300 - split);
      internet_state const expected = internet_update(internet_update(internet_state{}, head), tail);
      internet_state actual = internet_update<avx2>(internet_state{}, head.first(split / 2));
      actual = internet_update<avx2>(actual, head.subspan(split / 2));
      actual = internet_update<avx2>(actual, tail);
      ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
    }
  }
  // The checksum overloads and byte ranges instead of spans.
  std::uint16_t const value = internet_compute(buffer);
  EXPECT_EQ(internet_update<avx2>(value, std::span<std::byte const>{}), value);
  EXPECT_EQ(internet_update<avx2>(value, buffer), internet_update(value, buffer));
  EXPECT_EQ(internet_compute<avx2>(buffer), internet_compute(buffer));
}

#endif // CHECKSUM_INTERNET_X86_AVX2

} // namespace
