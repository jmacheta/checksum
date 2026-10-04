#ifndef CHECKSUM_TESTS_TEST_DATA_HPP
#define CHECKSUM_TESTS_TEST_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Fills data with the low byte of each xorshift32 step from 0x12345678 ^ seed. Seed 0 gives the message of the reference vectors.
constexpr void fill_random(std::span<std::byte> data, std::uint32_t seed = 0) noexcept;

// size bytes of fill_random().
inline std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed = 0);

constexpr void fill_random(std::span<std::byte> data, std::uint32_t seed) noexcept {
  std::uint32_t state = 0x12345678U ^ seed;
  for(auto &byte : data) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    byte = static_cast<std::byte>(state);
  }
}

inline std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed) {
  std::vector<std::byte> result(size);
  fill_random(result, seed);
  return result;
}

#endif // CHECKSUM_TESTS_TEST_DATA_HPP
