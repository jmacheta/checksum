// Internet checksum with a run-time isa_id: explicit and detected selection must give bit-identical results to the
// default strategy, and an unsupported or uncompiled variant must fall back to the default loop.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace checksum;

// The size bytes of buffer from offset: an unaligned start address for most offsets.
std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

// A value no isa_id names: asking for it must fall back like asking for an uncompiled variant.
isa_id const unknown_isa = static_cast<isa_id>(0x7F);

// Every variant this build can be asked for, valid or not: results must match the default either way.
std::vector<isa_id> all_isas() { return {isa_id::none, isa_id::x86_avx2, detect_isa(), unknown_isa}; }

TEST(internet_runtime, every_isa_matches_default_for_every_size_to_2048) {
  std::vector<std::byte> const buffer = random_bytes(2048 + 8, 1);
  for(isa_id const isa : all_isas()) {
    int const id = static_cast<int>(isa);
    for(std::size_t offset = 0; offset <= 7; ++offset) {
      for(std::size_t size = 0; size <= 2048; ++size) {
        std::span<std::byte const> const chunk = at(buffer, offset, size);
        ASSERT_EQ(internet_compute(chunk, isa), internet_compute(chunk)) << "isa " << id << ", offset " << offset << ", size " << size;
      }
    }
  }
}

TEST(internet_runtime, every_isa_matches_default_for_large_sizes) {
  std::vector<std::byte> const buffer = random_bytes(100'008, 2);
  for(isa_id const isa : all_isas()) {
    int const id = static_cast<int>(isa);
    for(std::size_t const size : {511UZ, 512UZ, 513UZ, 4096UZ, 10'000UZ, 65'536UZ, 99'999UZ}) {
      for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
        std::span<std::byte const> const chunk = at(buffer, offset, size);
        ASSERT_EQ(internet_compute(chunk, isa), internet_compute(chunk)) << "isa " << id << ", offset " << offset << ", size " << size;
      }
    }
  }
}

TEST(internet_runtime, updates_and_splits_match_default) {
  std::vector<std::byte> const buffer = random_bytes(3000, 3);
  for(isa_id const isa : all_isas()) {
    // One state through split updates, inside words and around the kernel threshold, at several start offsets.
    for(std::size_t const split : {1UZ, 63UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ}) {
      for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
        auto const data = at(buffer, offset, 1300);
        internet_state const expected = internet_update(internet_state{}, data);
        internet_state actual = internet_update(internet_state{}, data.first(split / 2), isa);
        actual = internet_update(actual, data.subspan(split / 2, split - (split / 2)), isa);
        actual = internet_update(actual, data.subspan(split), isa);
        ASSERT_EQ(actual, expected) << "isa " << static_cast<int>(isa) << ", split " << split << ", offset " << offset;
      }
    }
  }
}

TEST(internet_runtime, byte_ranges_and_checksum_overloads) {
  std::vector<std::byte> const buffer = random_bytes(5000, 4);
  std::vector<std::uint8_t> const chars(buffer.size(), 0xA5);
  for(isa_id const isa : all_isas()) {
    EXPECT_EQ(internet_compute(buffer, isa), internet_compute(buffer)) << "isa " << static_cast<int>(isa);
    EXPECT_EQ(internet_update(internet_state{}, buffer, isa), internet_update(internet_state{}, buffer)) << "isa " << static_cast<int>(isa);
    std::uint16_t const value = internet_compute(buffer);
    EXPECT_EQ(internet_update(value, std::span<std::byte const>{}, isa), value) << "isa " << static_cast<int>(isa);
    EXPECT_EQ(internet_update(value, buffer, isa), internet_update(value, buffer)) << "isa " << static_cast<int>(isa);
    EXPECT_EQ(internet_compute(chars, isa), internet_compute(chars)) << "isa " << static_cast<int>(isa);
  }
}

} // namespace
