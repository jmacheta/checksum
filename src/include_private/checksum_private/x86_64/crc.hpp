#ifndef CHECKSUM_PRIVATE_X86_64_CRC_HPP
#define CHECKSUM_PRIVATE_X86_64_CRC_HPP

// x86-64 kernels: PCLMULQDQ folding (needs SSE4.1; VPCLMULQDQ + AVX2 add a 256-bit loop from 256 bytes) and the SSE4.2
// crc32 instruction for CRC-32C. Included only by crc_arch.hpp.

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::crc_detail {

#if defined(__PCLMUL__) && defined(__SSE4_1__)
inline constexpr bool folding_available = true;
#else
inline constexpr bool folding_available = false;
#endif

// x86 has no instruction for the CRC-32 polynomial, only for CRC-32C.
template <std::uint32_t Polynomial> inline constexpr bool crc32_instructions_available = false;

#if defined(__SSE4_2__)
template <> inline constexpr bool crc32_instructions_available<crc32c_reflected_polynomial> = true;
#endif

// A crc32 stream is latency bound (3 cycles per instruction, up to three more for the last 1-7 bytes). On an Intel Core
// Ultra 7 155H it wins up to 24 bytes; folding is ~30 % faster at 31.
inline constexpr std::size_t crc32_folding_minimum_size = 25;

#if defined(__PCLMUL__) && defined(__SSE4_1__) && defined(__VPCLMULQDQ__) && defined(__AVX2__)
inline constexpr std::size_t wide_folding_minimum_size = 256;
#else
inline constexpr std::size_t wide_folding_minimum_size = std::numeric_limits<std::size_t>::max();
#endif

#if defined(__PCLMUL__) && defined(__SSE4_1__)
// Fold kernel of fold_blocks_with(): 128-bit vectors, low lane first.
struct pclmul_kernel {
  using vector = __m128i;

#if defined(__VPCLMULQDQ__) && defined(__AVX2__)
  static constexpr bool wide = true;
#else
  static constexpr bool wide = false;
#endif

  // One iteration of eight 256-bit accumulators.
  static constexpr std::size_t wide_minimum_size = wide_folding_minimum_size;

  // Byte 0 lowest if reflected, byte-reversed otherwise.
  template <bool Reflected> static vector load(std::span<std::byte const> data) noexcept;

  // 16-byte aligned load.
  static vector pair(folding_pair const &constants) noexcept;

  // The register in the first 64 message bits: low lane if reflected, high lane otherwise.
  template <bool Reflected> static vector state(std::uint64_t remainder) noexcept;

  template <bool Reflected> static vector first_block(std::span<std::byte const> data, std::uint64_t remainder) noexcept;

  // Low lane times the low constant xor high lane times the high constant, plus next.
  static vector fold(vector accumulator, vector constants, vector next) noexcept;

  // In message order, the accumulator's first size bytes (zero-extended in front) fold over one block onto its other
  // bytes followed by the last size message bytes.
  template <bool Reflected> static vector partial(vector accumulator, vector by_one, std::span<std::byte const> last, std::size_t size) noexcept;

  // Carry-less 64 x 64-bit product, low 64 bits in the low lane.
  static vector multiply(std::uint64_t first, std::uint64_t second) noexcept;

  template <int Lane> static std::uint64_t lane(vector value) noexcept;

  // A * x^64 mod P: the high half folded over 128 bits, then a Barrett reduction. Reflected products carry an extra
  // factor x (see make_folding_constants()), hence the one-bit shifts.
  template <bool Reflected> static std::uint64_t reduce(vector accumulator, folding_constants const &constants) noexcept;

#if defined(__VPCLMULQDQ__) && defined(__AVX2__)
  // Two blocks: byte 0 lowest if reflected, each block byte-reversed otherwise.
  template <bool Reflected> static __m256i load_two(std::span<std::byte const> data) noexcept;

  // fold() of two blocks with the constants of each lane.
  static __m256i fold_two(__m256i accumulators, __m256i constants, __m256i next) noexcept;

  // Eight 256-bit accumulators (blocks 2j, 2j + 1 in accumulator j) fold 256 bytes per step, then four fold 128 and
  // 64. Returns 8 blocks (2 per vector) with fewer than 64 bytes left after them; consumed: the bytes they cover.
  template <bool Reflected>
  static std::array<__m256i, 4> fold_wide_blocks(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data,
                                                 std::size_t &consumed) noexcept;

  // fold_wide_blocks(), its 8 blocks folded onto the last one in parallel, then fold_tail_with() on the last 0..63 bytes.
  template <bool Reflected>
  static vector fold_wide(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;
#endif
};
#endif

#if defined(__SSE4_2__)
template <>
[[gnu::always_inline]] inline std::uint32_t crc32_word<crc32c_reflected_polynomial>(std::uint32_t remainder, std::uint64_t word) noexcept {
  return static_cast<std::uint32_t>(_mm_crc32_u64(remainder, word));
}

template <>
[[gnu::always_inline]] inline std::uint32_t crc32_instructions<crc32c_reflected_polynomial>(std::uint32_t remainder,
                                                                                            std::span<std::byte const> data) noexcept {
  constexpr std::size_t word = 8;
  auto const update_word = [&](std::span<std::byte const> bytes) {
    remainder = crc32_word<crc32c_reflected_polynomial>(remainder, load_little_endian<word>(bytes));
  };
  // Four words per iteration, then 2 and 1: inputs below 32 bytes run no loop.
  while(data.size() >= 4 * word) {
    update_word(data);
    update_word(data.subspan(word));
    update_word(data.subspan(2 * word));
    update_word(data.subspan(3 * word));
    data = data.subspan(4 * word);
  }
  if(data.size() >= 2 * word) {
    update_word(data);
    update_word(data.subspan(word));
    data = data.subspan(2 * word);
  }
  if(data.size() >= word) {
    update_word(data);
    data = data.subspan(word);
  }
  // The last 0..7 bytes in at most three instructions.
  if(data.size() >= 4) {
    remainder = _mm_crc32_u32(remainder, static_cast<std::uint32_t>(load_little_endian<4>(data)));
    data = data.subspan(4);
  }
  if(data.size() >= 2) {
    remainder = _mm_crc32_u16(remainder, static_cast<std::uint16_t>(load_little_endian<2>(data)));
    data = data.subspan(2);
  }
  if(!data.empty()) {
    remainder = _mm_crc32_u8(remainder, std::to_integer<std::uint8_t>(data[0]));
  }
  return remainder;
}
#endif

#if defined(__PCLMUL__) && defined(__SSE4_1__)
template <bool Reflected> [[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::load(std::span<std::byte const> data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load, the intrinsic's interface.
  vector const block = _mm_loadu_si128(reinterpret_cast<vector const *>(data.data()));
  if constexpr(Reflected) {
    return block;
  } else {
    return _mm_shuffle_epi8(block, _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
  }
}

[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::pair(folding_pair const &constants) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): aligned vector load of a constant pair.
  return _mm_load_si128(reinterpret_cast<vector const *>(constants.data()));
}

template <bool Reflected> [[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::state(std::uint64_t remainder) noexcept {
  return Reflected ? _mm_cvtsi64_si128(static_cast<long long>(remainder)) : _mm_set_epi64x(static_cast<long long>(remainder), 0);
}

template <bool Reflected>
[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::first_block(std::span<std::byte const> data, std::uint64_t remainder) noexcept {
  return _mm_xor_si128(load<Reflected>(data), state<Reflected>(remainder));
}

[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::fold(vector accumulator, vector constants, vector next) noexcept {
  return _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(accumulator, constants, 0x00), _mm_clmulepi64_si128(accumulator, constants, 0x11)), next);
}

template <bool Reflected>
[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::partial(vector accumulator, vector by_one, std::span<std::byte const> last,
                                                                           std::size_t size) noexcept {
  vector const reverse = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
  vector const message = Reflected ? accumulator : _mm_shuffle_epi8(accumulator, reverse);
  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector loads, the intrinsic's interface.
  vector const tail = _mm_loadu_si128(reinterpret_cast<vector const *>(last.data()));
  vector const to_end = _mm_loadu_si128(reinterpret_cast<vector const *>(block_shift_indices.data() + size));
  vector const to_start = _mm_loadu_si128(reinterpret_cast<vector const *>(block_shift_indices.data() + folding_block_size + size));
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  vector const first = _mm_shuffle_epi8(message, to_end);
  vector const second = _mm_blendv_epi8(_mm_shuffle_epi8(message, to_start), tail, to_start);
  if constexpr(Reflected) {
    return fold(first, by_one, second);
  } else {
    return fold(_mm_shuffle_epi8(first, reverse), by_one, _mm_shuffle_epi8(second, reverse));
  }
}

[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::multiply(std::uint64_t first, std::uint64_t second) noexcept {
  return _mm_clmulepi64_si128(_mm_cvtsi64_si128(static_cast<long long>(first)), _mm_cvtsi64_si128(static_cast<long long>(second)), 0x00);
}

template <int Lane> [[gnu::always_inline]] inline std::uint64_t pclmul_kernel::lane(vector value) noexcept {
  return static_cast<std::uint64_t>(_mm_extract_epi64(value, Lane));
}

template <bool Reflected>
[[gnu::always_inline]] inline std::uint64_t pclmul_kernel::reduce(vector accumulator, folding_constants const &constants) noexcept {
  // Stays in vector registers: each move between them and general registers costs about 3 cycles of latency.
  vector const by_one = pair(constants.by_one);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): aligned load of the quotient and the polynomial.
  vector const barrett = _mm_load_si128(reinterpret_cast<vector const *>(&constants.quotient));
  if constexpr(Reflected) {
    // Low lane: the high half plus the low half folded over 64 bits; high lane: the rest of the fold.
    vector const value = _mm_xor_si128(_mm_clmulepi64_si128(accumulator, by_one, 0x10), _mm_srli_si128(accumulator, 8));
    vector const quotient = _mm_xor_si128(value, _mm_slli_epi64(_mm_clmulepi64_si128(value, barrett, 0x00), 1));
    vector const product = _mm_clmulepi64_si128(quotient, barrett, 0x10);
    // High lane of the 128-bit product shifted left by one.
    vector const shifted = _mm_xor_si128(_mm_slli_epi64(product, 1), _mm_srli_epi64(_mm_slli_si128(product, 8), 63));
    return lane<1>(_mm_xor_si128(value, shifted));
  } else {
    // High lane: the low half plus the high half folded over 64 bits; low lane: the rest of the fold.
    vector const value = _mm_xor_si128(_mm_clmulepi64_si128(accumulator, by_one, 0x01), _mm_slli_si128(accumulator, 8));
    vector const quotient = _mm_xor_si128(value, _mm_clmulepi64_si128(value, barrett, 0x01));
    return lane<0>(_mm_xor_si128(value, _mm_clmulepi64_si128(quotient, barrett, 0x11)));
  }
}

#if defined(__VPCLMULQDQ__) && defined(__AVX2__)
template <bool Reflected> [[gnu::always_inline]] inline __m256i pclmul_kernel::load_two(std::span<std::byte const> data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load, the intrinsic's interface.
  __m256i const blocks = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(data.data()));
  if constexpr(Reflected) {
    return blocks;
  } else {
    __m128i const reverse = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    return _mm256_shuffle_epi8(blocks, _mm256_broadcastsi128_si256(reverse));
  }
}

[[gnu::always_inline]] inline __m256i pclmul_kernel::fold_two(__m256i accumulators, __m256i constants, __m256i next) noexcept {
  return _mm256_xor_si256(
      _mm256_xor_si256(_mm256_clmulepi64_epi128(accumulators, constants, 0x00), _mm256_clmulepi64_epi128(accumulators, constants, 0x11)), next);
}

// Inlined only into the non-inlined long-input callers (fold_long_message(), fold_long_crc32()): a second call cost
// up to 10 % at 256 bytes.
template <bool Reflected>
[[gnu::always_inline]] inline std::array<__m256i, 4> pclmul_kernel::fold_wide_blocks(folding_constants const &constants, std::uint64_t remainder,
                                                                                     std::span<std::byte const> data,
                                                                                     std::size_t &consumed) noexcept {
  constexpr std::size_t half = 32;   // bytes per 256-bit accumulator
  constexpr std::size_t eight = 256; // bytes per iteration of eight accumulators
  constexpr std::size_t four = 128;  // bytes per iteration of four
  __m256i const by_eight = _mm256_broadcastsi128_si256(pair(constants.by_eight));
  static_assert(wide_minimum_size == eight, "the eight-accumulator loop runs at least once");
  __m256i const by_sixteen = _mm256_broadcastsi128_si256(pair(constants.by_sixteen));
  std::array<__m256i, 8> accumulators{};
  unrolled<8>([&](auto index) { accumulators[index] = load_two<Reflected>(data.subspan(index * half)); });
  accumulators[0] = _mm256_xor_si256(accumulators[0], _mm256_zextsi128_si256(state<Reflected>(remainder)));
  consumed = eight;
  // Branches that 256 bytes do not take are marked unlikely: each taken branch on that path cost about 2 %.
  if(data.size() - consumed >= eight) [[unlikely]] {
    do {
      unrolled<8>([&](auto index) {
        accumulators[index] = fold_two(accumulators[index], by_sixteen, load_two<Reflected>(data.subspan(consumed + (index * half))));
      });
      consumed += eight;
    } while(data.size() - consumed >= eight);
  }
  // Blocks 2j, 2j + 1 fold over eight blocks onto blocks 2j + 8, 2j + 9.
  std::array<__m256i, 4> pairs{};
  unrolled<4>([&](auto index) { pairs[index] = fold_two(accumulators[index], by_eight, accumulators[index + 4]); });
  if(data.size() - consumed >= four) [[unlikely]] {
    do {
      unrolled<4>([&](auto index) { pairs[index] = fold_two(pairs[index], by_eight, load_two<Reflected>(data.subspan(consumed + (index * half)))); });
      consumed += four;
    } while(data.size() - consumed >= four);
  }
  if(data.size() - consumed >= folding_four_blocks_size) [[unlikely]] {
    // Blocks 0..3 fold over four blocks onto blocks 4..7, followed by blocks 8..11.
    __m256i const by_four = _mm256_broadcastsi128_si256(pair(constants.by_four));
    pairs = {fold_two(pairs[0], by_four, pairs[2]), fold_two(pairs[1], by_four, pairs[3]), load_two<Reflected>(data.subspan(consumed)),
             load_two<Reflected>(data.subspan(consumed + half))};
    consumed += folding_four_blocks_size;
  }
  return pairs;
}

template <bool Reflected>
[[gnu::always_inline]] inline pclmul_kernel::vector pclmul_kernel::fold_wide(folding_constants const &constants, std::uint64_t remainder,
                                                                             std::span<std::byte const> data) noexcept {
  std::size_t consumed = 0;
  std::array<__m256i, 4> const pairs = fold_wide_blocks<Reflected>(constants, remainder, data, consumed);
  // Blocks 0..5 fold onto block 7 with the constants of their own distances, in parallel; block 6 folds over one.
  auto const distances = [](folding_pair const &longer) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned load of two adjacent constant pairs.
    return _mm256_loadu_si256(reinterpret_cast<__m256i const *>(longer.data()));
  };
  __m256i const block_7 = _mm256_blend_epi32(pairs[3], _mm256_setzero_si256(), 0x0F);
  __m256i const folded = fold_two(pairs[0], distances(constants.by_seven),
                                  fold_two(pairs[1], distances(constants.by_five), fold_two(pairs[2], distances(constants.by_three), block_7)));
  vector const accumulator = fold(_mm256_castsi256_si128(pairs[3]), pair(constants.by_one),
                                  _mm_xor_si128(_mm256_castsi256_si128(folded), _mm256_extracti128_si256(folded, 1)));
  if(consumed != data.size()) [[unlikely]] {
    return fold_tail_with<Reflected, pclmul_kernel>(accumulator, constants, data.subspan(consumed), data.last(folding_block_size));
  }
  return accumulator;
}
#endif

template <bool Reflected>
[[gnu::always_inline]] inline std::uint64_t fold_blocks(folding_constants const &constants, std::uint64_t remainder,
                                                        std::span<std::byte const> data) noexcept {
  return pclmul_kernel::reduce<Reflected>(fold_blocks_with<Reflected, pclmul_kernel>(constants, remainder, data), constants);
}

#if defined(__SSE4_2__)
template <>
[[gnu::always_inline]] inline std::uint32_t fold_crc32<crc32c_reflected_polynomial>(folding_constants const &constants, std::uint32_t remainder,
                                                                                    std::span<std::byte const> data) noexcept {
#if defined(__VPCLMULQDQ__) && defined(__AVX2__)
  if(data.size() >= wide_folding_minimum_size) {
    // Blocks 0..5 fold onto blocks 6 and 7 in parallel; crc32 instructions take the 32 bytes and the last 0..63 bytes:
    // 20 % faster at 256 bytes than folding onto one block.
    std::size_t consumed = 0;
    std::array<__m256i, 4> const pairs = pclmul_kernel::fold_wide_blocks<true>(constants, remainder, data, consumed);
    auto const both = [&](folding_pair const &distance) { return _mm256_broadcastsi128_si256(pclmul_kernel::pair(distance)); };
    __m256i const last_two = pclmul_kernel::fold_two(
        pairs[0], both(constants.by_six),
        pclmul_kernel::fold_two(pairs[1], both(constants.by_four), pclmul_kernel::fold_two(pairs[2], both(constants.by_two), pairs[3])));
    __m128i const low = _mm256_castsi256_si128(last_two);
    __m128i const high = _mm256_extracti128_si256(last_two, 1);
    std::uint32_t crc = crc32_word<crc32c_reflected_polynomial>(0, pclmul_kernel::lane<0>(low));
    crc = crc32_word<crc32c_reflected_polynomial>(crc, pclmul_kernel::lane<1>(low));
    crc = crc32_word<crc32c_reflected_polynomial>(crc, pclmul_kernel::lane<0>(high));
    crc = crc32_word<crc32c_reflected_polynomial>(crc, pclmul_kernel::lane<1>(high));
    if(consumed != data.size()) [[unlikely]] {
      crc = crc32_instructions<crc32c_reflected_polynomial>(crc, data.subspan(consumed));
    }
    return crc;
  }
#endif
  return fold_crc32_with<crc32c_reflected_polynomial, pclmul_kernel>(constants, remainder, data);
}
#endif
#endif

} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_X86_64_CRC_HPP
