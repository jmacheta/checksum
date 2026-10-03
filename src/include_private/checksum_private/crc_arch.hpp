#ifndef CHECKSUM_PRIVATE_CRC_ARCH_HPP
#define CHECKSUM_PRIVATE_CRC_ARCH_HPP

// CPU kernels of crc_lut_sliced and crc_lut_braided, from the directory arch.hpp selects; without the instruction-set
// extensions, or with CHECKSUM_ACCELERATION defined to 0, no kernel is used. Each architecture header defines:
// - folding_available; if true, fold_blocks() via fold_blocks_with() and its fold kernel
// - crc32_instructions_available<Polynomial>; where true, crc32_word<Polynomial>() and crc32_instructions<Polynomial>()
// - crc32_folding_minimum_size: shortest CRC-32/CRC-32C input folded before the CRC32 reduction (max: never)
// - wide_folding_minimum_size: shortest input for a loop wider than four 128-bit accumulators (max: none)

#include <checksum/crc.hpp>
#include <checksum_private/arch.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

// std::array of __m128i or uint64x2_t drops their __may_alias__ attribute, which arrays of accumulators do not need.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

namespace checksum::crc_detail {

inline constexpr std::size_t folding_block_size = 16;

inline constexpr std::size_t folding_four_blocks_size = 4 * folding_block_size;

// One block: on x86-64 the folding kernel is at least as fast as slicing-by-8 from 16 bytes.
inline constexpr std::size_t folding_minimum_size = folding_block_size;

// Shuffle indices (pshufb, tbl; 0x80 gives a zero byte): the 16 at offset r move the first r bytes to the end, the 16
// at offset 16 + r move the last 16 - r bytes to the start and mark the last r positions with 0x80.
alignas(16) inline constexpr std::array<std::uint8_t, 48> block_shift_indices{
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0,    1,    2,    3,    4,    5,    6,    7,
    8,    9,    10,   11,   12,   13,   14,   15,   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

// CRC-32 polynomial 0x04C11DB7, reflected.
inline constexpr std::uint32_t crc32_reflected_polynomial = 0xEDB88320;

// CRC-32C polynomial 0x1EDC6F41, reflected.
inline constexpr std::uint32_t crc32c_reflected_polynomial = 0x82F63B78;

// Calls body(integral_constant<I>) for I = 0..Count-1. Accumulator arrays indexed by constants stay in registers; a
// run-time loop over them keeps them in memory at -O2.
template <std::size_t Count, class Body> void unrolled(Body body) noexcept;

// First Size (1..8) bytes, first byte lowest; compilers fold it into one load (plus a byte swap on big-endian targets).
template <std::size_t Size> std::uint64_t load_little_endian(std::span<std::byte const> data) noexcept;

// First 8 bytes with the first message bit lowest (reflected) or highest. Byte swap after a little-endian load: not
// every compiler folds a direct big-endian assembly into one load.
template <bool Reflected> std::uint64_t load_word(std::span<std::byte const> data) noexcept;

// Folds at least 16 bytes into the register in 64-bit form (low-aligned if reflected, top-aligned otherwise). Inlined
// into every loop: one shared copy per bit order is up to 31 % (GCC) / 43 % (Clang) slower below 256 B on x86-64.
template <bool Reflected>
std::uint64_t fold_blocks(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;

// Updates a reflected 32-bit register with the target's CRC32/CRC32C instructions. Used for any parameter set with that
// polynomial, reflected input and a std::uint32_t register, whatever its initial value and final XOR.
template <std::uint32_t Polynomial> std::uint32_t crc32_instructions(std::uint32_t remainder, std::span<std::byte const> data) noexcept;

// One CRC32/CRC32C instruction over 8 message bytes, first byte lowest.
template <std::uint32_t Polynomial> std::uint32_t crc32_word(std::uint32_t remainder, std::uint64_t word) noexcept;

// Folds at least 16 bytes, then reduces the last 128 bits with two CRC32 instructions, shorter than a Barrett
// reduction. Defined where folding and the instructions exist and crc32_folding_minimum_size is finite.
template <std::uint32_t Polynomial>
std::uint32_t fold_crc32(folding_constants const &constants, std::uint32_t remainder, std::span<std::byte const> data) noexcept;

// Folding after Intel's "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ" (2009): four accumulators over
// 64-byte steps, then single and partial blocks; returns a block whose CRC from zero is the new register. Kernel:
// - vector: 128-bit type; in normal bit order a block holds the first message bit highest
// - load<R>(data): next 16-byte block
// - pair(constants): folding_pair, low lane first
// - first_block<R>(data, register): first block plus the register on its first 64 message bits
// - fold(accumulator, constants, next): accumulator folded over the distance of constants, plus next
// - partial<R>(accumulator, by_one, last, size): accumulator plus the last size (1..15) bytes; last = final 16 bytes
// - reduce<R>(accumulator, constants): the 64-bit register
// - lane<L>(accumulator): low (0) or high (1) 64 bits; fold_crc32_with() only
// - wide; if true, wide_minimum_size and fold_wide<R>(), a wider loop that finishes with fold_four_with()
template <bool Reflected, class Kernel>
typename Kernel::vector fold_blocks_with(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;

// Folds the remaining 64-byte groups into four accumulators (first block first), merges them, then fold_tail_with().
template <bool Reflected, class Kernel>
typename Kernel::vector fold_four_with(std::array<typename Kernel::vector, 4> accumulators, folding_constants const &constants,
                                       std::span<std::byte const> data, std::span<std::byte const> last) noexcept;

// Folds the remaining 16-byte blocks and the partial block into one accumulator; last is the final 16 message bytes.
template <bool Reflected, class Kernel>
typename Kernel::vector fold_tail_with(typename Kernel::vector accumulator, folding_constants const &constants, std::span<std::byte const> data,
                                       std::span<std::byte const> last) noexcept;

// fold_crc32() over a kernel: the last accumulator's CRC from zero, low 64 bits first, in two crc32_word() steps.
template <std::uint32_t Polynomial, class Kernel>
std::uint32_t fold_crc32_with(folding_constants const &constants, std::uint32_t remainder, std::span<std::byte const> data) noexcept;

template <std::size_t Count, class Body> [[gnu::always_inline]] inline void unrolled(Body body) noexcept {
  [&]<std::size_t... Index> [[gnu::always_inline]] (std::index_sequence<Index...>) {
    (body(std::integral_constant<std::size_t, Index>{}), ...);
  }(std::make_index_sequence<Count>{});
}

template <std::size_t Size> [[gnu::always_inline]] inline std::uint64_t load_little_endian(std::span<std::byte const> data) noexcept {
  return [&]<std::size_t... Index> [[gnu::always_inline]] (std::index_sequence<Index...>) {
    return ((std::uint64_t{std::to_integer<std::uint8_t>(data[Index])} << (byte_bits * Index)) | ...);
  }(std::make_index_sequence<Size>{});
}

template <bool Reflected> [[gnu::always_inline]] inline std::uint64_t load_word(std::span<std::byte const> data) noexcept {
  std::uint64_t const word = load_little_endian<word_size>(data);
  return Reflected ? word : std::byteswap(word);
}

template <bool Reflected, class Kernel>
[[gnu::always_inline]] inline typename Kernel::vector fold_tail_with(typename Kernel::vector accumulator, folding_constants const &constants,
                                                                     std::span<std::byte const> data, std::span<std::byte const> last) noexcept {
  typename Kernel::vector const by_one = Kernel::pair(constants.by_one);
  for(; data.size() >= folding_block_size; data = data.subspan(folding_block_size)) {
    accumulator = Kernel::fold(accumulator, by_one, Kernel::template load<Reflected>(data));
  }
  if(!data.empty()) {
    accumulator = Kernel::template partial<Reflected>(accumulator, by_one, last, data.size());
  }
  return accumulator;
}

template <bool Reflected, class Kernel>
[[gnu::always_inline]] inline typename Kernel::vector fold_four_with(std::array<typename Kernel::vector, 4> accumulators,
                                                                     folding_constants const &constants, std::span<std::byte const> data,
                                                                     std::span<std::byte const> last) noexcept {
  typename Kernel::vector const by_four = Kernel::pair(constants.by_four);
  typename Kernel::vector const by_one = Kernel::pair(constants.by_one);
  for(; data.size() >= folding_four_blocks_size; data = data.subspan(folding_four_blocks_size)) {
    unrolled<4>([&](auto index) {
      accumulators[index] = Kernel::fold(accumulators[index], by_four, Kernel::template load<Reflected>(data.subspan(index * folding_block_size)));
    });
  }
  typename Kernel::vector const accumulator =
      Kernel::fold(Kernel::fold(Kernel::fold(accumulators[0], by_one, accumulators[1]), by_one, accumulators[2]), by_one, accumulators[3]);
  return fold_tail_with<Reflected, Kernel>(accumulator, constants, data, last);
}

template <bool Reflected, class Kernel>
[[gnu::always_inline]] inline typename Kernel::vector fold_blocks_with(folding_constants const &constants, std::uint64_t remainder,
                                                                       std::span<std::byte const> data) noexcept {
  constexpr std::size_t block = folding_block_size;
  if constexpr(Kernel::wide) {
    if(data.size() >= Kernel::wide_minimum_size) {
      return Kernel::template fold_wide<Reflected>(constants, remainder, data);
    }
  }
  if(data.size() >= folding_four_blocks_size) {
    return fold_four_with<Reflected, Kernel>(
        {Kernel::template first_block<Reflected>(data, remainder), Kernel::template load<Reflected>(data.subspan(block)),
         Kernel::template load<Reflected>(data.subspan(2 * block)), Kernel::template load<Reflected>(data.subspan(3 * block))},
        constants, data.subspan(folding_four_blocks_size), data.last(block));
  }
  return fold_tail_with<Reflected, Kernel>(Kernel::template first_block<Reflected>(data, remainder), constants, data.subspan(block),
                                           data.last(block));
}

template <std::uint32_t Polynomial, class Kernel>
[[gnu::always_inline]] inline std::uint32_t fold_crc32_with(folding_constants const &constants, std::uint32_t remainder,
                                                            std::span<std::byte const> data) noexcept {
  typename Kernel::vector const accumulator = fold_blocks_with<true, Kernel>(constants, remainder, data);
  return crc32_word<Polynomial>(crc32_word<Polynomial>(0, Kernel::template lane<0>(accumulator)), Kernel::template lane<1>(accumulator));
}

} // namespace checksum::crc_detail

#if defined(CHECKSUM_ARCH_X86_64)
#include <checksum_private/x86_64/crc.hpp>
#elif defined(CHECKSUM_ARCH_ARM)
#include <checksum_private/arm/crc.hpp>
#elif defined(CHECKSUM_ARCH_RISCV64)
#include <checksum_private/riscv64/crc.hpp>
#else
#include <checksum_private/generic/crc.hpp>
#endif

#pragma GCC diagnostic pop

#endif // CHECKSUM_PRIVATE_CRC_ARCH_HPP
