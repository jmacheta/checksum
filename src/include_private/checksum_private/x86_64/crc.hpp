#ifndef CHECKSUM_PRIVATE_X86_64_CRC_HPP
#define CHECKSUM_PRIVATE_X86_64_CRC_HPP

// x86-64 kernels: PCLMULQDQ folding and the SSE4.2 crc32 instruction for CRC-32C from the x86_pclmul preset on, plus a 256-bit
// VPCLMULQDQ loop from 256 bytes from x86_vpclmul on. isa::generic (SSE2) has none. Included only by crc_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>
#include <checksum_private/vector_array.hpp>

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::crc_detail {
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header): a private header, included only by src/crc/*.cpp.
namespace {

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_pclmul))
struct folding_kernel<Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t minimum_size = folding_minimum_size;
  // One iteration of eight 256-bit accumulators.
  static constexpr std::size_t wide_minimum_size = isa_detail::includes(Isa, isa::x86_vpclmul) ? 256 : std::numeric_limits<std::size_t>::max();
  // Inlined into every loop: one shared copy per bit order is up to 31 % (GCC) / 43 % (Clang) slower below 256 B.
  template <bool Reflected>
  static std::uint64_t fold(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;
};

// x86 has no instruction for the CRC-32 polynomial, only for CRC-32C.
template <isa Isa, std::uint32_t Polynomial>
  requires(Polynomial == crc32c_reflected_polynomial && isa_detail::includes(Isa, isa::x86_pclmul))
struct crc32_kernel<Isa, Polynomial> {
  static constexpr bool available = true;
  // A crc32 stream is latency bound (3 cycles per instruction, up to three more for the last 1-7 bytes). On an Intel Core
  // Ultra 7 155H it wins up to 24 bytes; folding is ~30 % faster at 31.
  static constexpr std::size_t folding_minimum_size = 25;
  // One CRC32C instruction over 8 message bytes, first byte lowest.
  static std::uint32_t word(std::uint32_t remainder, std::uint64_t value) noexcept;
  static std::uint32_t update(std::uint32_t remainder, std::span<std::byte const> data) noexcept;
  // Folds at least 16 bytes, then reduces the last 128 bits with two CRC32 instructions, shorter than a Barrett reduction.
  static std::uint32_t fold(folding_constants const &constants, std::uint32_t remainder, std::span<std::byte const> data) noexcept;
};

// Fold kernel of fold_blocks_with(): 128-bit vectors, low lane first.
template <isa Isa> struct pclmul_kernel {
  using vector = __m128i;

  static constexpr bool wide = isa_detail::includes(Isa, isa::x86_vpclmul);

  static constexpr std::size_t wide_minimum_size = folding_kernel<Isa>::wide_minimum_size;

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

  // Two blocks: byte 0 lowest if reflected, each block byte-reversed otherwise. Only where wide.
  template <bool Reflected> static __m256i load_two(std::span<std::byte const> data) noexcept;

  // Two adjacent constant pairs, unaligned. A member, not a lambda: GCC compiles lambdas without the preset's AVX target.
  static __m256i two_pairs(folding_pair const &longer) noexcept;

  // fold() of two blocks with the constants of each lane.
  static __m256i fold_two(__m256i accumulators, __m256i constants, __m256i next) noexcept;

  // Eight 256-bit accumulators (blocks 2j, 2j + 1 in accumulator j) fold 256 bytes per step, then four fold 128 and
  // 64. Returns 8 blocks (2 per vector) with fewer than 64 bytes left after them; consumed: the bytes they cover.
  template <bool Reflected>
  static isa_detail::vector_array<__m256i, 4> fold_wide_blocks(folding_constants const &constants, std::uint64_t remainder,
                                                               std::span<std::byte const> data, std::size_t &consumed) noexcept;

  // fold_wide_blocks(), its 8 blocks folded onto the last one in parallel, then fold_tail_with() on the last 0..63 bytes.
  template <bool Reflected>
  static vector fold_wide(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;
};

template <isa Isa, std::uint32_t Polynomial>
  requires(Polynomial == crc32c_reflected_polynomial && isa_detail::includes(Isa, isa::x86_pclmul))
CHECKSUM_ALWAYS_INLINE inline std::uint32_t crc32_kernel<Isa, Polynomial>::word(std::uint32_t remainder, std::uint64_t value) noexcept {
  return static_cast<std::uint32_t>(_mm_crc32_u64(remainder, value));
}

template <isa Isa, std::uint32_t Polynomial>
  requires(Polynomial == crc32c_reflected_polynomial && isa_detail::includes(Isa, isa::x86_pclmul))
CHECKSUM_ALWAYS_INLINE inline std::uint32_t crc32_kernel<Isa, Polynomial>::update(std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  constexpr std::size_t word_bytes = 8;
  auto const update_word = [&](std::span<std::byte const> bytes) { remainder = word(remainder, load_little_endian<word_bytes>(bytes)); };
  // Four words per iteration, then 2 and 1: inputs below 32 bytes run no loop.
  while(data.size() >= 4 * word_bytes) {
    update_word(data);
    update_word(data.subspan(word_bytes));
    update_word(data.subspan(2 * word_bytes));
    update_word(data.subspan(3 * word_bytes));
    data = data.subspan(4 * word_bytes);
  }
  if(data.size() >= 2 * word_bytes) {
    update_word(data);
    update_word(data.subspan(word_bytes));
    data = data.subspan(2 * word_bytes);
  }
  if(data.size() >= word_bytes) {
    update_word(data);
    data = data.subspan(word_bytes);
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

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::load(std::span<std::byte const> data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load, the intrinsic's interface.
  vector const block = _mm_loadu_si128(reinterpret_cast<vector const *>(data.data()));
  if constexpr(Reflected) {
    return block;
  } else {
    return _mm_shuffle_epi8(block, _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
  }
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::pair(folding_pair const &constants) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): aligned vector load of a constant pair.
  return _mm_load_si128(reinterpret_cast<vector const *>(constants.data()));
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::state(std::uint64_t remainder) noexcept {
  return Reflected ? _mm_cvtsi64_si128(static_cast<long long>(remainder)) : _mm_set_epi64x(static_cast<long long>(remainder), 0);
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::first_block(std::span<std::byte const> data,
                                                                                         std::uint64_t remainder) noexcept {
  return _mm_xor_si128(load<Reflected>(data), state<Reflected>(remainder));
}

template <isa Isa>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::fold(vector accumulator, vector constants, vector next) noexcept {
  return _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(accumulator, constants, 0x00), _mm_clmulepi64_si128(accumulator, constants, 0x11)), next);
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::partial(vector accumulator, vector by_one,
                                                                                     std::span<std::byte const> last, std::size_t size) noexcept {
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

template <isa Isa>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::multiply(std::uint64_t first, std::uint64_t second) noexcept {
  return _mm_clmulepi64_si128(_mm_cvtsi64_si128(static_cast<long long>(first)), _mm_cvtsi64_si128(static_cast<long long>(second)), 0x00);
}

template <isa Isa> template <int Lane> CHECKSUM_ALWAYS_INLINE inline std::uint64_t pclmul_kernel<Isa>::lane(vector value) noexcept {
  return static_cast<std::uint64_t>(_mm_extract_epi64(value, Lane));
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline std::uint64_t pclmul_kernel<Isa>::reduce(vector accumulator, folding_constants const &constants) noexcept {
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

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline __m256i pclmul_kernel<Isa>::load_two(std::span<std::byte const> data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector load, the intrinsic's interface.
  __m256i const blocks = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(data.data()));
  if constexpr(Reflected) {
    return blocks;
  } else {
    __m128i const reverse = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    return _mm256_shuffle_epi8(blocks, _mm256_broadcastsi128_si256(reverse));
  }
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline __m256i pclmul_kernel<Isa>::two_pairs(folding_pair const &longer) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): unaligned load of two adjacent constant pairs.
  return _mm256_loadu_si256(reinterpret_cast<__m256i const *>(longer.data()));
}

template <isa Isa>
CHECKSUM_ALWAYS_INLINE inline __m256i pclmul_kernel<Isa>::fold_two(__m256i accumulators, __m256i constants, __m256i next) noexcept {
  return _mm256_xor_si256(
      _mm256_xor_si256(_mm256_clmulepi64_epi128(accumulators, constants, 0x00), _mm256_clmulepi64_epi128(accumulators, constants, 0x11)), next);
}

// Inlined only into the non-inlined long-input callers (fold_long_message(), fold_long_crc32()): a second call cost
// 8-15 % at 256 bytes.
template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline isa_detail::vector_array<__m256i, 4>
pclmul_kernel<Isa>::fold_wide_blocks(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data,
                                     std::size_t &consumed) noexcept {
  constexpr std::size_t half = 32;   // bytes per 256-bit accumulator
  constexpr std::size_t eight = 256; // bytes per iteration of eight accumulators
  constexpr std::size_t four = 128;  // bytes per iteration of four
  __m256i const by_eight = _mm256_broadcastsi128_si256(pair(constants.by_eight));
  static_assert(wide_minimum_size == eight, "the eight-accumulator loop runs at least once");
  __m256i const by_sixteen = _mm256_broadcastsi128_si256(pair(constants.by_sixteen));
  isa_detail::vector_array<__m256i, 8> accumulators{};
  unrolled<8>([&](auto index) { accumulators[index] = load_two<Reflected>(data.subspan(index * half)); });
  accumulators[0] = _mm256_xor_si256(accumulators[0], _mm256_zextsi128_si256(state<Reflected>(remainder)));
  consumed = eight;
  // Branches that 256 bytes do not take are marked unlikely, so that path runs without taken jumps.
  if(data.size() - consumed >= eight) [[unlikely]] {
    do {
      unrolled<8>([&](auto index) {
        accumulators[index] = fold_two(accumulators[index], by_sixteen, load_two<Reflected>(data.subspan(consumed + (index * half))));
      });
      consumed += eight;
    } while(data.size() - consumed >= eight);
  }
  // Blocks 2j, 2j + 1 fold over eight blocks onto blocks 2j + 8, 2j + 9.
  isa_detail::vector_array<__m256i, 4> pairs{};
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

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline pclmul_kernel<Isa>::vector pclmul_kernel<Isa>::fold_wide(folding_constants const &constants, std::uint64_t remainder,
                                                                                       std::span<std::byte const> data) noexcept {
  std::size_t consumed = 0;
  isa_detail::vector_array<__m256i, 4> const pairs = fold_wide_blocks<Reflected>(constants, remainder, data, consumed);
  // Blocks 0..5 fold onto block 7 with the constants of their own distances, in parallel; block 6 folds over one.
  __m256i const block_7 = _mm256_blend_epi32(pairs[3], _mm256_setzero_si256(), 0x0F);
  __m256i const folded = fold_two(pairs[0], two_pairs(constants.by_seven),
                                  fold_two(pairs[1], two_pairs(constants.by_five), fold_two(pairs[2], two_pairs(constants.by_three), block_7)));
  vector const accumulator = fold(_mm256_castsi256_si128(pairs[3]), pair(constants.by_one),
                                  _mm_xor_si128(_mm256_castsi256_si128(folded), _mm256_extracti128_si256(folded, 1)));
  if(consumed != data.size()) [[unlikely]] {
    return fold_tail_with<Reflected, pclmul_kernel>(accumulator, constants, data.subspan(consumed), data.last(folding_block_size));
  }
  return accumulator;
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_pclmul))
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline std::uint64_t folding_kernel<Isa>::fold(folding_constants const &constants, std::uint64_t remainder,
                                                                      std::span<std::byte const> data) noexcept {
  return pclmul_kernel<Isa>::template reduce<Reflected>(fold_blocks_with<Reflected, pclmul_kernel<Isa>>(constants, remainder, data), constants);
}

template <isa Isa, std::uint32_t Polynomial>
  requires(Polynomial == crc32c_reflected_polynomial && isa_detail::includes(Isa, isa::x86_pclmul))
CHECKSUM_ALWAYS_INLINE inline std::uint32_t crc32_kernel<Isa, Polynomial>::fold(folding_constants const &constants, std::uint32_t remainder,
                                                                                std::span<std::byte const> data) noexcept {
  using kernel = pclmul_kernel<Isa>;
  if constexpr(kernel::wide) {
    if(data.size() >= kernel::wide_minimum_size) {
      // Blocks 0..5 fold onto blocks 6 and 7 in parallel; crc32 instructions take the 32 bytes and the last 0..63 bytes,
      // two carry-less multiplications fewer than folding onto one block.
      std::size_t consumed = 0;
      isa_detail::vector_array<__m256i, 4> const pairs = kernel::template fold_wide_blocks<true>(constants, remainder, data, consumed);
      auto const both = [&](folding_pair const &distance) { return _mm256_broadcastsi128_si256(kernel::pair(distance)); };
      __m256i const last_two =
          kernel::fold_two(pairs[0], both(constants.by_six),
                           kernel::fold_two(pairs[1], both(constants.by_four), kernel::fold_two(pairs[2], both(constants.by_two), pairs[3])));
      __m128i const low = _mm256_castsi256_si128(last_two);
      __m128i const high = _mm256_extracti128_si256(last_two, 1);
      std::uint32_t crc = word(0, kernel::template lane<0>(low));
      crc = word(crc, kernel::template lane<1>(low));
      crc = word(crc, kernel::template lane<0>(high));
      crc = word(crc, kernel::template lane<1>(high));
      if(consumed != data.size()) [[unlikely]] {
        crc = update(crc, data.subspan(consumed));
      }
      return crc;
    }
  }
  return fold_crc32_with<Isa, Polynomial, kernel>(constants, remainder, data);
}

} // namespace
} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_X86_64_CRC_HPP
