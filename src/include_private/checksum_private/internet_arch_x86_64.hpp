#ifndef CHECKSUM_PRIVATE_INTERNET_ARCH_X86_64_HPP
#define CHECKSUM_PRIVATE_INTERNET_ARCH_X86_64_HPP

// AVX2 kernel of the Internet checksum. Included only by internet_arch.hpp.

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

inline constexpr bool vector_sum_available = true;

// Below 256 bytes the kernel is no faster than the portable loop.
inline constexpr std::size_t vector_sum_minimum_size = 256;

inline constexpr std::size_t vector_block_size = 64;

// Each 64-bit lane takes one 32-bit half word per block: passes of 2^26 blocks keep the lanes and their merged sum below 2^61.
inline constexpr std::size_t vector_pass_blocks = std::size_t{1} << 26U;

// 64-byte blocks of 32-bit words, each split into the halves of a 64-bit lane.
inline std::uint64_t vector_sum(std::span<std::byte const> &data) noexcept {
  __m256i const low_halves = _mm256_set1_epi64x(0xFFFFFFFF);
  std::uint64_t total = 0;
  while(data.size() >= vector_block_size) {
    std::size_t const blocks = std::min(data.size() / vector_block_size, vector_pass_blocks);
    __m256i sum0 = _mm256_setzero_si256();
    __m256i sum1 = sum0;
    __m256i sum2 = sum0;
    __m256i sum3 = sum0;
    std::byte const *position = data.data();
    for(std::size_t block = 0; block < blocks; ++block, position += vector_block_size) {
      __m256i const first = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(position));
      __m256i const second = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(position + 32));
      sum0 = _mm256_add_epi64(sum0, _mm256_and_si256(first, low_halves));
      sum1 = _mm256_add_epi64(sum1, _mm256_srli_epi64(first, 32));
      sum2 = _mm256_add_epi64(sum2, _mm256_and_si256(second, low_halves));
      sum3 = _mm256_add_epi64(sum3, _mm256_srli_epi64(second, 32));
    }
    data = data.subspan(blocks * vector_block_size);
    __m256i const sum = _mm256_add_epi64(_mm256_add_epi64(sum0, sum1), _mm256_add_epi64(sum2, sum3));
    __m128i const half = _mm_add_epi64(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    total += fold(static_cast<std::uint64_t>(_mm_cvtsi128_si64(half)));
    total += fold(static_cast<std::uint64_t>(_mm_extract_epi64(half, 1)));
  }
  return total;
}

} // namespace checksum::internet_detail

#endif // CHECKSUM_PRIVATE_INTERNET_ARCH_X86_64_HPP
