#ifndef CHECKSUM_PRIVATE_X86_64_FLETCHER4_HPP
#define CHECKSUM_PRIVATE_X86_64_FLETCHER4_HPP

// x86-64 kernel of the fletcher4 lanes: the four lanes in one 256-bit AVX2 vector per sum from x86_avx2, else in two
// 128-bit SSE2 vectors (the x86-64 baseline). Included only by fletcher4_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <immintrin.h>

#include <cstddef>

namespace checksum::fletcher4_detail {

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
struct lane_kernel<Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t minimum_size = isa_detail::includes(Isa, isa::x86_avx2) ? 128 : 192;
  static lane_sums sum(std::byte const *data, std::size_t groups) noexcept;
};

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
lane_sums lane_kernel<Isa>::sum(std::byte const *data, std::size_t groups) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    __m256i sum1 = _mm256_setzero_si256();
    __m256i sum2 = sum1;
    __m256i sum3 = sum1;
    __m256i sum4 = sum1;
    for(; groups != 0; --groups, data += lane_count * word_size) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load.
      sum1 = _mm256_add_epi64(sum1, _mm256_cvtepu32_epi64(_mm_loadu_si128(reinterpret_cast<__m128i const *>(data))));
      sum2 = _mm256_add_epi64(sum2, sum1);
      sum3 = _mm256_add_epi64(sum3, sum2);
      sum4 = _mm256_add_epi64(sum4, sum3);
    }
    lane_sums lanes{};
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector stores.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(lanes[0].data()), sum1);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(lanes[1].data()), sum2);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(lanes[2].data()), sum3);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(lanes[3].data()), sum4);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    return lanes;
  } else {
    // Lanes 0 and 1 in the low vectors, 2 and 3 in the high ones.
    __m128i const zero = _mm_setzero_si128();
    __m128i sum1_low = zero;
    __m128i sum1_high = zero;
    __m128i sum2_low = zero;
    __m128i sum2_high = zero;
    __m128i sum3_low = zero;
    __m128i sum3_high = zero;
    __m128i sum4_low = zero;
    __m128i sum4_high = zero;
    for(; groups != 0; --groups, data += lane_count * word_size) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load.
      __m128i const words = _mm_loadu_si128(reinterpret_cast<__m128i const *>(data));
      sum1_low = _mm_add_epi64(sum1_low, _mm_unpacklo_epi32(words, zero));
      sum1_high = _mm_add_epi64(sum1_high, _mm_unpackhi_epi32(words, zero));
      sum2_low = _mm_add_epi64(sum2_low, sum1_low);
      sum2_high = _mm_add_epi64(sum2_high, sum1_high);
      sum3_low = _mm_add_epi64(sum3_low, sum2_low);
      sum3_high = _mm_add_epi64(sum3_high, sum2_high);
      sum4_low = _mm_add_epi64(sum4_low, sum3_low);
      sum4_high = _mm_add_epi64(sum4_high, sum3_high);
    }
    lane_sums lanes{};
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector stores.
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[0].data()), sum1_low);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[0].data() + 2), sum1_high);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[1].data()), sum2_low);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[1].data() + 2), sum2_high);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[2].data()), sum3_low);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[2].data() + 2), sum3_high);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[3].data()), sum4_low);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(lanes[3].data() + 2), sum4_high);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    return lanes;
  }
}

} // namespace checksum::fletcher4_detail

#endif // CHECKSUM_PRIVATE_X86_64_FLETCHER4_HPP
