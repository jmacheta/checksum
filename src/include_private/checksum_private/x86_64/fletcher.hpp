#ifndef CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP
#define CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP

// x86-64 kernels of the Fletcher checksums and Adler-32: SSE2 vectors for isa::generic, SSSE3 weighting from x86_pclmul,
// AVX2 vectors from x86_avx2, and vpdpbusd from x86_avxvnni. Included only by fletcher_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace checksum::fletcher_detail {

template <isa Isa> using x86_vector = std::conditional_t<isa_detail::includes(Isa, isa::x86_avx2), __m256i, __m128i>;
template <isa Isa> inline constexpr std::size_t x86_vector_size = sizeof(x86_vector<Isa>);

// Bytes 0, 1, ... of a vector weighted by its size, size - 1, ..., 1; each 32-bit lane sums its own bytes.
template <isa Isa> x86_vector<Isa> weighted_bytes(x86_vector<Isa> value) noexcept;

// The 16-bit or 32-bit values of the low or high half of each 128-bit lane, zero-extended to twice their size.
template <isa Isa> x86_vector<Isa> widen16_low(x86_vector<Isa> value) noexcept;
template <isa Isa> x86_vector<Isa> widen16_high(x86_vector<Isa> value) noexcept;
template <isa Isa> x86_vector<Isa> widen32_low(x86_vector<Isa> value) noexcept;
template <isa Isa> x86_vector<Isa> widen32_high(x86_vector<Isa> value) noexcept;

// Each 64-bit lane: the sum of its 8 bytes.
template <isa Isa> x86_vector<Isa> byte_sums(x86_vector<Isa> value) noexcept;

template <isa Isa> x86_vector<Isa> add_lanes32(x86_vector<Isa> first, x86_vector<Isa> second) noexcept;
template <isa Isa> x86_vector<Isa> add_lanes64(x86_vector<Isa> first, x86_vector<Isa> second) noexcept;
template <isa Isa> x86_vector<Isa> load_vector(std::byte const *data) noexcept;

// The lanes of a vector, stored with an intrinsic: GCC keeps an accumulator whose address reaches memcpy in memory, with a
// store and a load per block.
template <isa Isa, class Lane> std::array<Lane, x86_vector_size<Isa> / sizeof(Lane)> vector_lanes(x86_vector<Isa> value) noexcept;

// Sum of the lanes of Lane bits.
template <isa Isa, class Lane> std::uint64_t lane_total(x86_vector<Isa> value) noexcept;

// Column sums of widened values weighted by values_per_block minus their index in the block. With n lanes per 128 bits, lane j
// of low holds value (j / n) * 2n + j % n, and lane j of high the value n places later.
template <isa Isa, class Lane> std::uint64_t column_total(x86_vector<Isa> low, x86_vector<Isa> high, std::uint64_t values_per_block) noexcept;

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
struct block_kernel<8, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = x86_vector_size<Isa>;
  // A 32-bit lane of the previous sums gains at most 8 * 255 * j at block j.
  static constexpr std::size_t max_blocks = 1024;
  static_assert(std::uint64_t{8} * 255 * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // From 64 bytes the kernel beats the portable loops on a Core Ultra 7 155H with GCC and Clang, SSE2 and AVX2. The one
  // exception is Fletcher-16 with GCC and SSE2: 7 % slower at 64 bytes, faster from 96.
  static constexpr std::size_t minimum_size = 64;
  static constexpr std::size_t alignment = 1;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
struct block_kernel<16, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = x86_vector_size<Isa>;
  // A 32-bit lane of the previous sums gains at most 2 * 65535 * j at block j.
  static constexpr std::size_t max_blocks = 128;
  static_assert(std::uint64_t{2} * 65'535 * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // Where the kernel overtakes the portable loop on a Core Ultra 7 155H: 96 bytes with Clang, 128-160 with GCC.
  static constexpr std::size_t minimum_size = 128;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_avx2))
struct block_kernel<32, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = x86_vector_size<Isa>;
  // The 64-bit lanes have room for far more; 65536 values per chunk keep the weighted sum of the chunk within 64 bits.
  static constexpr std::size_t max_blocks = 65'536 / (block_size / 4);
  // Where the kernel overtakes the portable loop on a Core Ultra 7 155H: 160 bytes with Clang, 384 with GCC.
  static constexpr std::size_t minimum_size = 384;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

} // namespace checksum::fletcher_detail

namespace checksum::fletcher_detail {

template <isa Isa> x86_vector<Isa> weighted_bytes(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    // pmaddubsw adds pairs of byte products, at most 255 * (32 + 31), without saturating 16 bits.
    x86_vector<Isa> const weights =
        _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    return _mm256_madd_epi16(_mm256_maddubs_epi16(value, weights), _mm256_set1_epi16(1));
  } else if constexpr(isa_detail::includes(Isa, isa::x86_pclmul)) {
    // pmaddubsw adds pairs of byte products, at most 255 * (16 + 15), without saturating 16 bits.
    x86_vector<Isa> const weights = _mm_setr_epi8(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    return _mm_madd_epi16(_mm_maddubs_epi16(value, weights), _mm_set1_epi16(1));
  } else {
    x86_vector<Isa> const zero = _mm_setzero_si128();
    x86_vector<Isa> const low = _mm_madd_epi16(_mm_unpacklo_epi8(value, zero), _mm_setr_epi16(16, 15, 14, 13, 12, 11, 10, 9));
    x86_vector<Isa> const high = _mm_madd_epi16(_mm_unpackhi_epi8(value, zero), _mm_setr_epi16(8, 7, 6, 5, 4, 3, 2, 1));
    return _mm_add_epi32(low, high);
  }
}

template <isa Isa> x86_vector<Isa> widen16_low(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_unpacklo_epi16(value, _mm256_setzero_si256());
  } else {
    return _mm_unpacklo_epi16(value, _mm_setzero_si128());
  }
}

template <isa Isa> x86_vector<Isa> widen16_high(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_unpackhi_epi16(value, _mm256_setzero_si256());
  } else {
    return _mm_unpackhi_epi16(value, _mm_setzero_si128());
  }
}

template <isa Isa> x86_vector<Isa> widen32_low(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_unpacklo_epi32(value, _mm256_setzero_si256());
  } else {
    return _mm_unpacklo_epi32(value, _mm_setzero_si128());
  }
}

template <isa Isa> x86_vector<Isa> widen32_high(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_unpackhi_epi32(value, _mm256_setzero_si256());
  } else {
    return _mm_unpackhi_epi32(value, _mm_setzero_si128());
  }
}

template <isa Isa> x86_vector<Isa> add_lanes32(x86_vector<Isa> first, x86_vector<Isa> second) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_add_epi32(first, second);
  } else {
    return _mm_add_epi32(first, second);
  }
}

template <isa Isa> x86_vector<Isa> add_lanes64(x86_vector<Isa> first, x86_vector<Isa> second) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_add_epi64(first, second);
  } else {
    return _mm_add_epi64(first, second);
  }
}

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector loads, the intrinsics' interface.
template <isa Isa> x86_vector<Isa> load_vector(std::byte const *data) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_loadu_si256(reinterpret_cast<x86_vector<Isa> const *>(data));
  } else {
    return _mm_loadu_si128(reinterpret_cast<x86_vector<Isa> const *>(data));
  }
}

template <isa Isa, class Lane> std::array<Lane, x86_vector_size<Isa> / sizeof(Lane)> vector_lanes(x86_vector<Isa> value) noexcept {
  std::array<Lane, x86_vector_size<Isa> / sizeof(Lane)> lanes{};
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    _mm256_storeu_si256(reinterpret_cast<x86_vector<Isa> *>(lanes.data()), value);
  } else {
    _mm_storeu_si128(reinterpret_cast<x86_vector<Isa> *>(lanes.data()), value);
  }
  return lanes;
}
// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

template <isa Isa> x86_vector<Isa> byte_sums(x86_vector<Isa> value) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avx2)) {
    return _mm256_sad_epu8(value, _mm256_setzero_si256());
  } else {
    return _mm_sad_epu8(value, _mm_setzero_si128());
  }
}

template <isa Isa, class Lane> std::uint64_t lane_total(x86_vector<Isa> value) noexcept {
  std::uint64_t total = 0;
  for(Lane const lane : vector_lanes<Isa, Lane>(value)) {
    total += lane;
  }
  return total;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the low and high halves of the lanes.
template <isa Isa, class Lane> std::uint64_t column_total(x86_vector<Isa> low, x86_vector<Isa> high, std::uint64_t values_per_block) noexcept {
  constexpr std::size_t lanes = x86_vector_size<Isa> / sizeof(Lane);
  constexpr std::size_t per_half = 16 / sizeof(Lane);
  auto const low_lanes = vector_lanes<Isa, Lane>(low);
  auto const high_lanes = vector_lanes<Isa, Lane>(high);
  std::uint64_t total = 0;
  for(std::size_t lane = 0; lane < lanes; ++lane) {
    std::uint64_t const index = ((lane / per_half) * 2 * per_half) + (lane % per_half);
    total += (values_per_block - index) * std::uint64_t{low_lanes[lane]};
    total += (values_per_block - index - per_half) * std::uint64_t{high_lanes[lane]};
  }
  return total;
}

template <isa Isa>
  requires(target_detail::any_preset<Isa>)
chunk_sums block_kernel<8, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  if constexpr(isa_detail::includes(Isa, isa::x86_avxvnni)) {
    // vpdpbusd weights four blocks by 127, 126, ..., 0 with one instruction per block, and adding the byte sums once at the end
    // makes the weights 128 .. 1; so previous gains the sums only every four blocks. Four accumulators hide the latency of
    // vpdpbusd. The last blocks weigh 31 .. 0 and count their previous per block.
    x86_vector<Isa> const zero = _mm256_setzero_si256();
    x86_vector<Isa> const first_weights = _mm256_setr_epi8(127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110,
                                                           109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96);
    x86_vector<Isa> const second_weights = _mm256_setr_epi8(95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74,
                                                            73, 72, 71, 70, 69, 68, 67, 66, 65, 64);
    x86_vector<Isa> const third_weights = _mm256_setr_epi8(63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
                                                           40, 39, 38, 37, 36, 35, 34, 33, 32);
    x86_vector<Isa> const fourth_weights =
        _mm256_setr_epi8(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    // The byte sums of psadbw are 64-bit lanes.
    x86_vector<Isa> sum = zero;
    x86_vector<Isa> group_previous = zero;
    x86_vector<Isa> weighted = zero;
    x86_vector<Isa> second_weighted = zero;
    x86_vector<Isa> third_weighted = zero;
    x86_vector<Isa> fourth_weighted = zero;
    for(; blocks >= 4; blocks -= 4, data += 4 * block_size) {
      x86_vector<Isa> const first = load_vector<Isa>(data);
      x86_vector<Isa> const second = load_vector<Isa>(data + block_size);
      x86_vector<Isa> const third = load_vector<Isa>(data + (2 * block_size));
      x86_vector<Isa> const fourth = load_vector<Isa>(data + (3 * block_size));
      group_previous = _mm256_add_epi64(group_previous, sum);
      sum = _mm256_add_epi64(sum, _mm256_add_epi64(_mm256_add_epi64(byte_sums<Isa>(first), byte_sums<Isa>(second)),
                                                   _mm256_add_epi64(byte_sums<Isa>(third), byte_sums<Isa>(fourth))));
      weighted = _mm256_dpbusd_avx_epi32(weighted, first, first_weights);
      second_weighted = _mm256_dpbusd_avx_epi32(second_weighted, second, second_weights);
      third_weighted = _mm256_dpbusd_avx_epi32(third_weighted, third, third_weights);
      fourth_weighted = _mm256_dpbusd_avx_epi32(fourth_weighted, fourth, fourth_weights);
    }
    weighted = add_lanes32<Isa>(add_lanes32<Isa>(weighted, second_weighted), add_lanes32<Isa>(third_weighted, fourth_weighted));
    x86_vector<Isa> previous = zero;
    for(; blocks != 0; --blocks, data += block_size) {
      x86_vector<Isa> const value = load_vector<Isa>(data);
      previous = _mm256_add_epi64(previous, sum);
      sum = _mm256_add_epi64(sum, byte_sums<Isa>(value));
      weighted = _mm256_dpbusd_avx_epi32(weighted, value, fourth_weights);
    }
    // In 64-bit lanes: 128 * group_previous + 32 * previous + both halves of weighted + sum, then both totals at once.
    x86_vector<Isa> const total =
        _mm256_add_epi64(_mm256_add_epi64(_mm256_slli_epi64(group_previous, 7), _mm256_slli_epi64(previous, 5)),
                         _mm256_add_epi64(_mm256_add_epi64(_mm256_srli_epi64(weighted, 32), _mm256_blend_epi32(weighted, zero, 0xAA)), sum));
    x86_vector<Isa> const pairs = _mm256_add_epi64(_mm256_unpacklo_epi64(sum, total), _mm256_unpackhi_epi64(sum, total));
    __m128i const totals = _mm_add_epi64(_mm256_castsi256_si128(pairs), _mm256_extracti128_si256(pairs, 1));
    return {.sum = static_cast<std::uint64_t>(_mm_cvtsi128_si64(totals)), .weighted = static_cast<std::uint64_t>(_mm_extract_epi64(totals, 1))};
  } else {
    // Per block: previous += sum, so that previous ends as the sum over blocks of the byte sums before each block. The byte sums
    // stay below 2^32, so their 64-bit lanes of psadbw add as 32-bit lanes.
    x86_vector<Isa> sum{};
    x86_vector<Isa> previous{};
    x86_vector<Isa> weighted{};
    for(; blocks != 0; --blocks, data += block_size) {
      x86_vector<Isa> const value = load_vector<Isa>(data);
      previous = add_lanes32<Isa>(previous, sum);
      sum = add_lanes32<Isa>(sum, byte_sums<Isa>(value));
      weighted = add_lanes32<Isa>(weighted, weighted_bytes<Isa>(value));
    }
    return {.sum = lane_total<Isa, std::uint32_t>(sum),
            .weighted = (block_size * lane_total<Isa, std::uint32_t>(previous)) + lane_total<Isa, std::uint32_t>(weighted)};
  }
}

// Column sums of the values, weighted at the end of the chunk; previous as for bytes.
template <isa Isa>
  requires(target_detail::any_preset<Isa>)
chunk_sums block_kernel<16, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  constexpr std::uint64_t values_per_block = block_size / 2;
  x86_vector<Isa> sum{};
  x86_vector<Isa> previous{};
  x86_vector<Isa> low_columns{};
  x86_vector<Isa> high_columns{};
  for(; blocks != 0; --blocks, data += block_size) {
    x86_vector<Isa> const value = load_vector<Isa>(data);
    x86_vector<Isa> const low = widen16_low<Isa>(value);
    x86_vector<Isa> const high = widen16_high<Isa>(value);
    previous = add_lanes32<Isa>(previous, sum);
    sum = add_lanes32<Isa>(sum, add_lanes32<Isa>(low, high));
    low_columns = add_lanes32<Isa>(low_columns, low);
    high_columns = add_lanes32<Isa>(high_columns, high);
  }
  return {.sum = lane_total<Isa, std::uint32_t>(sum),
          .weighted = (values_per_block * lane_total<Isa, std::uint32_t>(previous)) +
                      column_total<Isa, std::uint32_t>(low_columns, high_columns, values_per_block)};
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_avx2))
chunk_sums block_kernel<32, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  constexpr std::uint64_t values_per_block = block_size / 4;
  x86_vector<Isa> sum{};
  x86_vector<Isa> previous{};
  x86_vector<Isa> low_columns{};
  x86_vector<Isa> high_columns{};
  for(; blocks != 0; --blocks, data += block_size) {
    x86_vector<Isa> const value = load_vector<Isa>(data);
    x86_vector<Isa> const low = widen32_low<Isa>(value);
    x86_vector<Isa> const high = widen32_high<Isa>(value);
    previous = add_lanes64<Isa>(previous, sum);
    sum = add_lanes64<Isa>(sum, add_lanes64<Isa>(low, high));
    low_columns = add_lanes64<Isa>(low_columns, low);
    high_columns = add_lanes64<Isa>(high_columns, high);
  }
  return {.sum = lane_total<Isa, std::uint64_t>(sum),
          .weighted = (values_per_block * lane_total<Isa, std::uint64_t>(previous)) +
                      column_total<Isa, std::uint64_t>(low_columns, high_columns, values_per_block)};
}

} // namespace checksum::fletcher_detail

#endif // CHECKSUM_PRIVATE_X86_64_FLETCHER_HPP
