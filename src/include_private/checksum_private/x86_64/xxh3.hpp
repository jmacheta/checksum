#ifndef CHECKSUM_PRIVATE_X86_64_XXH3_HPP
#define CHECKSUM_PRIVATE_X86_64_XXH3_HPP

// SSE2 and AVX2 kernels of the XXH3 stripe loop: SSE2 is the x86-64 baseline, AVX2 runs from the x86_avx2 preset. Included only by
// xxh3_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>
#include <checksum_private/vector_array.hpp>

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace checksum::xxh3_detail {

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
struct stripe_kernel<Isa> {
  static constexpr bool available = true;
  // Both kernels beat the portable loop from 256 bytes, the shortest size measured, with GCC and Clang on a Core Ultra 7 155H.
  static constexpr std::size_t minimum_size = 0;
  using lanes =
      std::conditional_t<isa_detail::includes(Isa, isa::x86_avx2), isa_detail::vector_array<__m256i, 2>, isa_detail::vector_array<__m128i, 4>>;
  static lanes load(accumulator_array const &accumulators) noexcept;
  static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  static void scramble(lanes &values, std::byte const *secret) noexcept;
};

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector loads and stores, the intrinsics' interface.

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
auto stripe_kernel<Isa>::load(accumulator_array const &accumulators) noexcept -> lanes {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return {_mm256_loadu_si256(reinterpret_cast<__m256i const *>(accumulators.data())),
            _mm256_loadu_si256(reinterpret_cast<__m256i const *>(accumulators.data() + 4))};
  } else {
    lanes values{};
    for(std::size_t index = 0; index < values.size(); ++index) {
      values[index] = _mm_loadu_si128(reinterpret_cast<__m128i const *>(accumulators.data() + (2 * index)));
    }
    return values;
  }
}

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
void stripe_kernel<Isa>::store(accumulator_array &accumulators, lanes const &values) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(accumulators.data()), values[0]);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(accumulators.data() + 4), values[1]);
    return;
  } else {
    for(std::size_t index = 0; index < values.size(); ++index) {
      _mm_storeu_si128(reinterpret_cast<__m128i *>(accumulators.data() + (2 * index)), values[index]);
    }
  }
}

// Each lane adds the product of the 32-bit halves of data ^ secret, and the data of its neighbor.
template <isa Isa>
  requires(target_detail::any_preset<Isa>)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the stripe and the secret are both bytes.
void stripe_kernel<Isa>::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    for(std::size_t index = 0; index < values.size(); ++index) {
      __m256i const data = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(stripe) + index);
      __m256i const key = _mm256_xor_si256(data, _mm256_loadu_si256(reinterpret_cast<__m256i const *>(secret) + index));
      __m256i const product = _mm256_mul_epu32(key, _mm256_srli_epi64(key, 32));
      values[index] = _mm256_add_epi64(values[index], _mm256_add_epi64(product, _mm256_shuffle_epi32(data, _MM_SHUFFLE(1, 0, 3, 2))));
    }
    return;
  } else {
    for(std::size_t index = 0; index < values.size(); ++index) {
      __m128i const data = _mm_loadu_si128(reinterpret_cast<__m128i const *>(stripe) + index);
      __m128i const key = _mm_xor_si128(data, _mm_loadu_si128(reinterpret_cast<__m128i const *>(secret) + index));
      __m128i const product = _mm_mul_epu32(key, _mm_shuffle_epi32(key, _MM_SHUFFLE(0, 3, 0, 1)));
      values[index] = _mm_add_epi64(values[index], _mm_add_epi64(product, _mm_shuffle_epi32(data, _MM_SHUFFLE(1, 0, 3, 2))));
    }
  }
}

// The 64-bit product with the 32-bit prime, from the products of both halves.
template <isa Isa>
  requires(target_detail::any_preset<Isa>)
void stripe_kernel<Isa>::scramble(lanes &values, std::byte const *secret) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    __m256i const prime = _mm256_set1_epi32(static_cast<int>(primes_32::prime_1));
    for(std::size_t index = 0; index < values.size(); ++index) {
      __m256i const mixed = _mm256_xor_si256(values[index], _mm256_srli_epi64(values[index], scramble_shift));
      __m256i const key = _mm256_xor_si256(mixed, _mm256_loadu_si256(reinterpret_cast<__m256i const *>(secret) + index));
      __m256i const high = _mm256_mul_epu32(_mm256_srli_epi64(key, 32), prime);
      values[index] = _mm256_add_epi64(_mm256_mul_epu32(key, prime), _mm256_slli_epi64(high, 32));
    }
    return;
  } else {
    __m128i const prime = _mm_set1_epi32(static_cast<int>(primes_32::prime_1));
    for(std::size_t index = 0; index < values.size(); ++index) {
      __m128i const mixed = _mm_xor_si128(values[index], _mm_srli_epi64(values[index], scramble_shift));
      __m128i const key = _mm_xor_si128(mixed, _mm_loadu_si128(reinterpret_cast<__m128i const *>(secret) + index));
      __m128i const high = _mm_mul_epu32(_mm_srli_epi64(key, 32), prime);
      values[index] = _mm_add_epi64(_mm_mul_epu32(key, prime), _mm_slli_epi64(high, 32));
    }
  }
}

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace checksum::xxh3_detail

#endif // CHECKSUM_PRIVATE_X86_64_XXH3_HPP
