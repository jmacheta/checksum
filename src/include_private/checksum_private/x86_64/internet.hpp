#ifndef CHECKSUM_PRIVATE_X86_64_INTERNET_HPP
#define CHECKSUM_PRIVATE_X86_64_INTERNET_HPP

// AVX2 kernel of the Internet checksum, for the presets from x86_avx2 on, and the add-with-carry chain of the portable loop.
// Included only by internet_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <immintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_avx2))
struct block_kernel<Isa> {
  static constexpr bool available = true;
  // Below about 350 bytes the portable loop is faster on a Core Ultra 7 155H: the kernel pays for the call and the reduction.
  static constexpr std::size_t minimum_size = 512;
  static block_total sum(std::span<std::byte const> data) noexcept;
};

// GCC and Clang turn __builtin_addcll into an adc chain, MSVC _addcarry_u64; GCC 14 on Arm materializes every carry instead.
template <isa Isa>
  requires(target_detail::any_preset<Isa>)
struct carry_chain<Isa> {
  static constexpr bool available = true;
  static word add(word left, word right, word &carry) noexcept;
};

inline constexpr std::size_t vector_block_size = 64;

// Each 64-bit lane takes one 32-bit half word per block: passes of 2^26 blocks keep the lanes and their merged sum below 2^61.
inline constexpr std::size_t vector_pass_blocks = std::size_t{1} << 26U;

// 64-byte blocks of 32-bit words, each split into the halves of a 64-bit lane.
template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_avx2))
block_total block_kernel<Isa>::sum(std::span<std::byte const> data) noexcept {
  std::size_t const message_size = data.size();
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
  return {.sum = total, .size = message_size - data.size()};
}

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
word carry_chain<Isa>::add(word left, word right, word &carry) noexcept {
  word sum = 0;
#if defined(_MSC_VER) && !defined(__clang__)
  carry = _addcarry_u64(static_cast<unsigned char>(carry), left, right, &sum);
#else
  unsigned long long carry_out = 0;
  sum = static_cast<word>(__builtin_addcll(left, right, carry, &carry_out));
  carry = static_cast<word>(carry_out);
#endif
  return sum;
}

} // namespace checksum::internet_detail

#endif // CHECKSUM_PRIVATE_X86_64_INTERNET_HPP
