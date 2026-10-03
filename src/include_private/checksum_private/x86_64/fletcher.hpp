#ifndef CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP
#define CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP

// x86-64 kernels of the Fletcher checksums and Adler-32 on 256-bit AVX2 vectors, else on 128-bit SSE2 vectors (the x86-64
// baseline) for bytes and 16-bit values. Included only by fletcher_arch.hpp.

#include <checksum_private/generic/fletcher.hpp>

#if defined(__SSE2__)

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace checksum::fletcher_detail {

#if defined(__AVX2__)
using simd_vector = __m256i;
#else
using simd_vector = __m128i;
#endif

inline constexpr std::size_t simd_size = sizeof(simd_vector);

// Bytes 0, 1, ... of a vector weighted by simd_size, simd_size - 1, ..., 1; each 32-bit lane sums its own bytes.
inline simd_vector weighted_bytes(simd_vector value) noexcept;

// The 16-bit or 32-bit values of the low or high half of each 128-bit lane, zero-extended to twice their size.
inline simd_vector widen16_low(simd_vector value) noexcept;
inline simd_vector widen16_high(simd_vector value) noexcept;
inline simd_vector widen32_low(simd_vector value) noexcept;
inline simd_vector widen32_high(simd_vector value) noexcept;

// Each 64-bit lane: the sum of its 8 bytes.
inline simd_vector byte_sums(simd_vector value) noexcept;

inline simd_vector add_lanes32(simd_vector first, simd_vector second) noexcept;
inline simd_vector add_lanes64(simd_vector first, simd_vector second) noexcept;
inline simd_vector load_vector(std::byte const *data) noexcept;

// Sum of the lanes of Lane bits.
template <class Lane> std::uint64_t lane_total(simd_vector value) noexcept;

// Column sums of widened values weighted by values_per_block minus their index in the block. With n lanes per 128 bits, lane j
// of low holds value (j / n) * 2n + j % n, and lane j of high the value n places later.
template <class Lane> std::uint64_t column_total(simd_vector low, simd_vector high, std::uint64_t values_per_block) noexcept;

template <> struct kernel<8> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = simd_size;
  // A 32-bit lane of the previous sums gains at most 8 * 255 * j at block j.
  static constexpr std::size_t max_blocks = 1024;
  static_assert(std::uint64_t{8 * 255} * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // From 64 bytes the kernel beats the portable loops on a Core Ultra 7 155H with GCC and Clang, SSE2 and AVX2. The one
  // exception is Fletcher-16 with GCC and SSE2: 7 % slower at 64 bytes, faster from 96.
  static constexpr std::size_t minimum_size = 64;
  static constexpr std::size_t alignment = 1;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

template <> struct kernel<16> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = simd_size;
  // A 32-bit lane of the previous sums gains at most 2 * 65535 * j at block j.
  static constexpr std::size_t max_blocks = 128;
  static_assert(std::uint64_t{2 * 65'535} * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // Where the kernel overtakes the portable loop on a Core Ultra 7 155H: 96 bytes with Clang, 128-160 with GCC.
  static constexpr std::size_t minimum_size = 128;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

#if defined(__AVX2__)
template <> struct kernel<32> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = simd_size;
  // The 64-bit lanes have room for far more; 65536 values per chunk keep the weighted sum of the chunk within 64 bits.
  static constexpr std::size_t max_blocks = 65'536 / (block_size / 4);
  // Where the kernel overtakes the portable loop on a Core Ultra 7 155H: 160 bytes with Clang, 384 with GCC.
  static constexpr std::size_t minimum_size = 384;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};
#endif

} // namespace checksum::fletcher_detail

namespace checksum::fletcher_detail {

#if defined(__AVX2__)

inline simd_vector weighted_bytes(simd_vector value) noexcept {
  // pmaddubsw adds pairs of byte products, at most 255 * (32 + 31), without saturating 16 bits.
  simd_vector const weights =
      _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
  return _mm256_madd_epi16(_mm256_maddubs_epi16(value, weights), _mm256_set1_epi16(1));
}

inline simd_vector widen16_low(simd_vector value) noexcept { return _mm256_unpacklo_epi16(value, _mm256_setzero_si256()); }
inline simd_vector widen16_high(simd_vector value) noexcept { return _mm256_unpackhi_epi16(value, _mm256_setzero_si256()); }
inline simd_vector widen32_low(simd_vector value) noexcept { return _mm256_unpacklo_epi32(value, _mm256_setzero_si256()); }
inline simd_vector widen32_high(simd_vector value) noexcept { return _mm256_unpackhi_epi32(value, _mm256_setzero_si256()); }
inline simd_vector add_lanes32(simd_vector first, simd_vector second) noexcept { return _mm256_add_epi32(first, second); }
inline simd_vector add_lanes64(simd_vector first, simd_vector second) noexcept { return _mm256_add_epi64(first, second); }
inline simd_vector load_vector(std::byte const *data) noexcept { return _mm256_loadu_si256(reinterpret_cast<simd_vector const *>(data)); }
inline simd_vector byte_sums(simd_vector value) noexcept { return _mm256_sad_epu8(value, _mm256_setzero_si256()); }

#else

inline simd_vector weighted_bytes(simd_vector value) noexcept {
#if defined(__SSSE3__)
  // pmaddubsw adds pairs of byte products, at most 255 * (16 + 15), without saturating 16 bits.
  simd_vector const weights = _mm_setr_epi8(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
  return _mm_madd_epi16(_mm_maddubs_epi16(value, weights), _mm_set1_epi16(1));
#else
  simd_vector const zero = _mm_setzero_si128();
  simd_vector const low = _mm_madd_epi16(_mm_unpacklo_epi8(value, zero), _mm_setr_epi16(16, 15, 14, 13, 12, 11, 10, 9));
  simd_vector const high = _mm_madd_epi16(_mm_unpackhi_epi8(value, zero), _mm_setr_epi16(8, 7, 6, 5, 4, 3, 2, 1));
  return _mm_add_epi32(low, high);
#endif
}

inline simd_vector widen16_low(simd_vector value) noexcept { return _mm_unpacklo_epi16(value, _mm_setzero_si128()); }
inline simd_vector widen16_high(simd_vector value) noexcept { return _mm_unpackhi_epi16(value, _mm_setzero_si128()); }
inline simd_vector widen32_low(simd_vector value) noexcept { return _mm_unpacklo_epi32(value, _mm_setzero_si128()); }
inline simd_vector widen32_high(simd_vector value) noexcept { return _mm_unpackhi_epi32(value, _mm_setzero_si128()); }
inline simd_vector add_lanes32(simd_vector first, simd_vector second) noexcept { return _mm_add_epi32(first, second); }
inline simd_vector add_lanes64(simd_vector first, simd_vector second) noexcept { return _mm_add_epi64(first, second); }
inline simd_vector load_vector(std::byte const *data) noexcept { return _mm_loadu_si128(reinterpret_cast<simd_vector const *>(data)); }
inline simd_vector byte_sums(simd_vector value) noexcept { return _mm_sad_epu8(value, _mm_setzero_si128()); }

#endif

template <class Lane> std::uint64_t lane_total(simd_vector value) noexcept {
  std::array<Lane, simd_size / sizeof(Lane)> lanes{};
  std::memcpy(lanes.data(), &value, simd_size);
  std::uint64_t total = 0;
  for(Lane const lane : lanes) {
    total += lane;
  }
  return total;
}

template <class Lane> std::uint64_t column_total(simd_vector low, simd_vector high, std::uint64_t values_per_block) noexcept {
  constexpr std::size_t lanes = simd_size / sizeof(Lane);
  constexpr std::size_t per_half = 16 / sizeof(Lane);
  std::array<Lane, lanes> low_lanes{};
  std::array<Lane, lanes> high_lanes{};
  std::memcpy(low_lanes.data(), &low, simd_size);
  std::memcpy(high_lanes.data(), &high, simd_size);
  std::uint64_t total = 0;
  for(std::size_t lane = 0; lane < lanes; ++lane) {
    std::uint64_t const index = ((lane / per_half) * 2 * per_half) + (lane % per_half);
    total += (values_per_block - index) * std::uint64_t{low_lanes[lane]};
    total += (values_per_block - index - per_half) * std::uint64_t{high_lanes[lane]};
  }
  return total;
}

// Per block: previous += sum, so that previous ends as the sum over blocks of the byte sums before each block. The byte sums
// stay below 2^32, so their 64-bit lanes of psadbw add as 32-bit lanes.
inline chunk_sums kernel<8>::sum(std::byte const *data, std::size_t blocks) noexcept {
  simd_vector sum{};
  simd_vector previous{};
  simd_vector weighted{};
  for(; blocks != 0; --blocks, data += block_size) {
    simd_vector const value = load_vector(data);
    previous = add_lanes32(previous, sum);
    sum = add_lanes32(sum, byte_sums(value));
    weighted = add_lanes32(weighted, weighted_bytes(value));
  }
  return {.sum = lane_total<std::uint32_t>(sum),
          .weighted = (block_size * lane_total<std::uint32_t>(previous)) + lane_total<std::uint32_t>(weighted)};
}

// Column sums of the values, weighted at the end of the chunk; previous as for bytes.
inline chunk_sums kernel<16>::sum(std::byte const *data, std::size_t blocks) noexcept {
  constexpr std::uint64_t values_per_block = block_size / 2;
  simd_vector sum{};
  simd_vector previous{};
  simd_vector low_columns{};
  simd_vector high_columns{};
  for(; blocks != 0; --blocks, data += block_size) {
    simd_vector const value = load_vector(data);
    simd_vector const low = widen16_low(value);
    simd_vector const high = widen16_high(value);
    previous = add_lanes32(previous, sum);
    sum = add_lanes32(sum, add_lanes32(low, high));
    low_columns = add_lanes32(low_columns, low);
    high_columns = add_lanes32(high_columns, high);
  }
  return {.sum = lane_total<std::uint32_t>(sum),
          .weighted =
              (values_per_block * lane_total<std::uint32_t>(previous)) + column_total<std::uint32_t>(low_columns, high_columns, values_per_block)};
}

#if defined(__AVX2__)
inline chunk_sums kernel<32>::sum(std::byte const *data, std::size_t blocks) noexcept {
  constexpr std::uint64_t values_per_block = block_size / 4;
  simd_vector sum{};
  simd_vector previous{};
  simd_vector low_columns{};
  simd_vector high_columns{};
  for(; blocks != 0; --blocks, data += block_size) {
    simd_vector const value = load_vector(data);
    simd_vector const low = widen32_low(value);
    simd_vector const high = widen32_high(value);
    previous = add_lanes64(previous, sum);
    sum = add_lanes64(sum, add_lanes64(low, high));
    low_columns = add_lanes64(low_columns, low);
    high_columns = add_lanes64(high_columns, high);
  }
  return {.sum = lane_total<std::uint64_t>(sum),
          .weighted =
              (values_per_block * lane_total<std::uint64_t>(previous)) + column_total<std::uint64_t>(low_columns, high_columns, values_per_block)};
}
#endif

} // namespace checksum::fletcher_detail

#endif

#endif // CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP
