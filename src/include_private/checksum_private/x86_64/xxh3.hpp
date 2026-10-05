#ifndef CHECKSUM_PRIVATE_X86_64_XXH3_HPP
#define CHECKSUM_PRIVATE_X86_64_XXH3_HPP

// SSE2 and AVX2 kernels of the XXH3 stripe loop: SSE2 is part of x86-64, AVX2 runs where the flags enable it. Included only by
// xxh3_arch.hpp.

#if !defined(__SSE2__)
#include <checksum_private/generic/xxh3.hpp>
#else

#include <immintrin.h>

#include <array>
#include <cstddef>

namespace checksum::xxh3_detail {

inline constexpr bool stripe_kernel_available = true;

// Both kernels beat the portable loop from 256 bytes, the shortest size measured, with GCC and Clang on a Core Ultra 7 155H.
inline constexpr std::size_t stripe_kernel_minimum_size = 0;

// Four 128-bit vectors of two lanes each.
struct sse2_kernel {
  using lanes = std::array<__m128i, 4>;
  static lanes load(accumulator_array const &accumulators) noexcept;
  static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  static void scramble(lanes &values, std::byte const *secret) noexcept;
};

#if defined(__AVX2__)

// Two 256-bit vectors of four lanes each.
struct avx2_kernel {
  using lanes = std::array<__m256i, 2>;
  static lanes load(accumulator_array const &accumulators) noexcept;
  static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  static void scramble(lanes &values, std::byte const *secret) noexcept;
};

using stripe_kernel = avx2_kernel;

#else

using stripe_kernel = sse2_kernel;

#endif

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector loads and stores, the intrinsics' interface.

inline sse2_kernel::lanes sse2_kernel::load(accumulator_array const &accumulators) noexcept {
  lanes values{};
  for(std::size_t index = 0; index < values.size(); ++index) {
    values[index] = _mm_loadu_si128(reinterpret_cast<__m128i const *>(accumulators.data() + (2 * index)));
  }
  return values;
}

inline void sse2_kernel::store(accumulator_array &accumulators, lanes const &values) noexcept {
  for(std::size_t index = 0; index < values.size(); ++index) {
    _mm_storeu_si128(reinterpret_cast<__m128i *>(accumulators.data() + (2 * index)), values[index]);
  }
}

// Each lane adds the product of the 32-bit halves of data ^ secret, and the data of its neighbor.
inline void sse2_kernel::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  for(std::size_t index = 0; index < values.size(); ++index) {
    __m128i const data = _mm_loadu_si128(reinterpret_cast<__m128i const *>(stripe) + index);
    __m128i const key = _mm_xor_si128(data, _mm_loadu_si128(reinterpret_cast<__m128i const *>(secret) + index));
    __m128i const product = _mm_mul_epu32(key, _mm_shuffle_epi32(key, _MM_SHUFFLE(0, 3, 0, 1)));
    values[index] = _mm_add_epi64(values[index], _mm_add_epi64(product, _mm_shuffle_epi32(data, _MM_SHUFFLE(1, 0, 3, 2))));
  }
}

// The 64-bit product with the 32-bit prime, from the products of both halves.
inline void sse2_kernel::scramble(lanes &values, std::byte const *secret) noexcept {
  __m128i const prime = _mm_set1_epi32(static_cast<int>(primes_32::prime_1));
  for(std::size_t index = 0; index < values.size(); ++index) {
    __m128i const mixed = _mm_xor_si128(values[index], _mm_srli_epi64(values[index], scramble_shift));
    __m128i const key = _mm_xor_si128(mixed, _mm_loadu_si128(reinterpret_cast<__m128i const *>(secret) + index));
    __m128i const high = _mm_mul_epu32(_mm_srli_epi64(key, 32), prime);
    values[index] = _mm_add_epi64(_mm_mul_epu32(key, prime), _mm_slli_epi64(high, 32));
  }
}

#if defined(__AVX2__)

inline avx2_kernel::lanes avx2_kernel::load(accumulator_array const &accumulators) noexcept {
  return {_mm256_loadu_si256(reinterpret_cast<__m256i const *>(accumulators.data())),
          _mm256_loadu_si256(reinterpret_cast<__m256i const *>(accumulators.data() + 4))};
}

inline void avx2_kernel::store(accumulator_array &accumulators, lanes const &values) noexcept {
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(accumulators.data()), values[0]);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(accumulators.data() + 4), values[1]);
}

inline void avx2_kernel::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  for(std::size_t index = 0; index < values.size(); ++index) {
    __m256i const data = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(stripe) + index);
    __m256i const key = _mm256_xor_si256(data, _mm256_loadu_si256(reinterpret_cast<__m256i const *>(secret) + index));
    __m256i const product = _mm256_mul_epu32(key, _mm256_srli_epi64(key, 32));
    values[index] = _mm256_add_epi64(values[index], _mm256_add_epi64(product, _mm256_shuffle_epi32(data, _MM_SHUFFLE(1, 0, 3, 2))));
  }
}

inline void avx2_kernel::scramble(lanes &values, std::byte const *secret) noexcept {
  __m256i const prime = _mm256_set1_epi32(static_cast<int>(primes_32::prime_1));
  for(std::size_t index = 0; index < values.size(); ++index) {
    __m256i const mixed = _mm256_xor_si256(values[index], _mm256_srli_epi64(values[index], scramble_shift));
    __m256i const key = _mm256_xor_si256(mixed, _mm256_loadu_si256(reinterpret_cast<__m256i const *>(secret) + index));
    __m256i const high = _mm256_mul_epu32(_mm256_srli_epi64(key, 32), prime);
    values[index] = _mm256_add_epi64(_mm256_mul_epu32(key, prime), _mm256_slli_epi64(high, 32));
  }
}

#endif

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace checksum::xxh3_detail

#endif

#endif // CHECKSUM_PRIVATE_X86_64_XXH3_HPP
