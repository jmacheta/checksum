#ifndef CHECKSUM_TESTS_TEST_DATA_HPP
#define CHECKSUM_TESTS_TEST_DATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Fills data with the low byte of each xorshift32 step from 0x12345678 ^ seed. Seed 0 gives the message of the reference vectors.
constexpr void fill_random(std::span<std::byte> data, std::uint32_t seed = 0) noexcept;

// size bytes of fill_random().
inline std::vector<std::byte> random_bytes(std::size_t size, std::uint32_t seed = 0);

// Size bytes of fill_random(), usable during constant evaluation.
template <std::size_t Size> constexpr std::array<std::byte, Size> random_array(std::uint32_t seed = 0) noexcept;

// The size bytes of buffer from offset: an unaligned start address for most offsets.
inline std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size);

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

template <std::size_t Size> constexpr std::array<std::byte, Size> random_array(std::uint32_t seed) noexcept {
  std::array<std::byte, Size> result{};
  fill_random(result, seed);
  return result;
}

inline std::span<std::byte const> at(std::vector<std::byte> const &buffer, std::size_t offset, std::size_t size) {
  return std::span<std::byte const>(buffer).subspan(offset, size);
}

#endif // CHECKSUM_TESTS_TEST_DATA_HPP
