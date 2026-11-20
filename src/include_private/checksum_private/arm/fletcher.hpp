#ifndef CHECKSUM_PRIVATE_ARM_FLETCHER_HPP
#define CHECKSUM_PRIVATE_ARM_FLETCHER_HPP

// Arm kernels of the Fletcher checksums and Adler-32, included only by fletcher_arch.hpp: NEON on little-endian AArch64
// and on the thumb2_neon/thumb2_crc presets, else the DSP instructions for bytes on little-endian 32-bit Arm on thumb2_dsp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#if defined(CHECKSUM_ARM64) || CHECKSUM_PRESET_HAS(NEON)

#include <arm_neon.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::fletcher_detail {

using target_detail::arm64_target;
using target_detail::neon_target;

// Sums of the lanes, in 64 bits. Not overloads: MSVC has one type for every NEON vector.
template <isa Isa> std::uint64_t lane_total_32(uint32x4_t value) noexcept;
template <isa Isa> std::uint64_t lane_total_64(uint64x2_t value) noexcept;

template <isa Isa>
  requires(neon_target<Isa>)
struct block_kernel<8, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // A 16-bit lane of the column sums gains at most 255 per block.
  static constexpr std::size_t max_blocks = 256;
  static_assert(255 * max_blocks <= std::numeric_limits<std::uint16_t>::max());
  // Faster than the portable loop from 64 bytes on a Cortex-A72 (Raspberry Pi 4), in AArch64 and AArch32.
  static constexpr std::size_t minimum_size = 64;
  static constexpr std::size_t alignment = 1;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

#if defined(CHECKSUM_ARM64)
// AArch32 has half the NEON registers: there the group loop spilled and ran 10-27 % slower than block_kernel<8>.
template <isa Isa>
  requires(neon_target<Isa>)
struct group_kernel<8, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // The weighted sum of a chunk, at most 255 * n(n + 1) / 2 for n bytes, stays within 32 bits, so the chunk loop reduces in
  // 32-bit arithmetic. A 16-bit lane of the column sums gains at most 255 per 4 blocks, and 255 per block in the last 3.
  static constexpr std::size_t max_blocks = 360;
  static_assert(std::uint64_t{255} * (block_size * max_blocks) * ((block_size * max_blocks) + 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  static_assert(255 * ((max_blocks / 4) + 3) <= std::numeric_limits<std::uint16_t>::max());
  // Where it overtakes block_kernel<8> on a Cortex-A72 (Raspberry Pi 4); below, its 64 column weights cost more than they save.
  static constexpr std::size_t minimum_size = 320;
  static constexpr std::size_t alignment = 1;
  static chunk_sums sum(std::byte const *start, std::size_t blocks) noexcept;
};
#endif

template <isa Isa>
  requires(neon_target<Isa>)
struct block_kernel<16, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // A 32-bit lane of the previous sums gains at most 2 * 65535 * j at block j.
  static constexpr std::size_t max_blocks = 128;
  static_assert(std::uint64_t{2 * 65'535} * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // Slower than the portable loop at 64 bytes on a Cortex-A72 (Raspberry Pi 4), in AArch64 and AArch32.
  static constexpr std::size_t minimum_size = 128;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

template <isa Isa>
  requires(neon_target<Isa>)
struct block_kernel<32, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // The 64-bit lanes have room for far more; 65536 values per chunk keep the weighted sum of the chunk within 64 bits.
  static constexpr std::size_t max_blocks = 65'536 / 4;
  // Threshold measured on a Cortex-A72 (Raspberry Pi 4).
  static constexpr std::size_t minimum_size = 256;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

} // namespace checksum::fletcher_detail

namespace checksum::fletcher_detail {

template <isa Isa> std::uint64_t lane_total_64(uint64x2_t value) noexcept { return vgetq_lane_u64(value, 0) + vgetq_lane_u64(value, 1); }

template <isa Isa> std::uint64_t lane_total_32(uint32x4_t value) noexcept { return lane_total_64<Isa>(vpaddlq_u32(value)); }

// Per block: previous += sum, so that previous ends as the sum over blocks of the byte sums before each block. Column sums of
// the 16 bytes get their weights 16 .. 1 at the end of the chunk.
template <isa Isa>
  requires(neon_target<Isa>)
chunk_sums block_kernel<8, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  static constexpr std::array<std::uint16_t, 16> weights{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  auto const *position = reinterpret_cast<std::uint8_t const *>(data);
  uint32x4_t sum = vdupq_n_u32(0);
  uint32x4_t previous = sum;
  uint16x8_t low_columns = vdupq_n_u16(0);
  uint16x8_t high_columns = low_columns;
  for(; blocks != 0; --blocks, position += block_size) {
    uint8x16_t const value = vld1q_u8(position);
    previous = vaddq_u32(previous, sum);
    sum = vpadalq_u16(sum, vpaddlq_u8(value));
    low_columns = vaddw_u8(low_columns, vget_low_u8(value));
    high_columns = vaddw_u8(high_columns, vget_high_u8(value));
  }
  uint16x8_t const low_weights = vld1q_u16(weights.data());
  uint16x8_t const high_weights = vld1q_u16(weights.data() + 8);
  uint32x4_t weighted = vmull_u16(vget_low_u16(low_columns), vget_low_u16(low_weights));
  weighted = vmlal_u16(weighted, vget_high_u16(low_columns), vget_high_u16(low_weights));
  weighted = vmlal_u16(weighted, vget_low_u16(high_columns), vget_low_u16(high_weights));
  weighted = vmlal_u16(weighted, vget_high_u16(high_columns), vget_high_u16(high_weights));
  return {.sum = lane_total_32<Isa>(sum), .weighted = (block_size * lane_total_32<Isa>(previous)) + lane_total_32<Isa>(weighted)};
}

#if defined(CHECKSUM_ARM64)
// As block_kernel<8> per 64 bytes, with pairwise additions of the 4 blocks into sum and 64 columns weighted 64 .. 1. The last
// blocks that fill no group add to the columns of the last 16 bytes, weights 16 .. 1, and count their previous per block.
template <isa Isa>
  requires(neon_target<Isa>)
chunk_sums group_kernel<8, Isa>::sum(std::byte const *start, std::size_t blocks) noexcept {
  auto const *data = reinterpret_cast<std::uint8_t const *>(start);
  static constexpr std::array<std::uint16_t, 64> weights{64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43,
                                                         42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21,
                                                         20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1};
  uint32x4_t sum = vdupq_n_u32(0);
  uint32x4_t previous = sum;
  uint16x8_t columns0 = vdupq_n_u16(0);
  uint16x8_t columns1 = columns0;
  uint16x8_t columns2 = columns0;
  uint16x8_t columns3 = columns0;
  uint16x8_t columns4 = columns0;
  uint16x8_t columns5 = columns0;
  uint16x8_t columns6 = columns0;
  uint16x8_t columns7 = columns0;
  for(; blocks >= 4; blocks -= 4, data += 4 * block_size) {
    uint8x16_t const first = vld1q_u8(data);
    uint8x16_t const second = vld1q_u8(data + block_size);
    uint8x16_t const third = vld1q_u8(data + (2 * block_size));
    uint8x16_t const fourth = vld1q_u8(data + (3 * block_size));
    previous = vaddq_u32(previous, sum);
    sum = vpadalq_u16(sum, vpadalq_u8(vpadalq_u8(vpadalq_u8(vpaddlq_u8(first), second), third), fourth));
    columns0 = vaddw_u8(columns0, vget_low_u8(first));
    columns1 = vaddw_u8(columns1, vget_high_u8(first));
    columns2 = vaddw_u8(columns2, vget_low_u8(second));
    columns3 = vaddw_u8(columns3, vget_high_u8(second));
    columns4 = vaddw_u8(columns4, vget_low_u8(third));
    columns5 = vaddw_u8(columns5, vget_high_u8(third));
    columns6 = vaddw_u8(columns6, vget_low_u8(fourth));
    columns7 = vaddw_u8(columns7, vget_high_u8(fourth));
  }
  // Counted in blocks: 4 per group.
  previous = vshlq_n_u32(previous, 2);
  for(; blocks != 0; --blocks, data += block_size) {
    uint8x16_t const value = vld1q_u8(data);
    previous = vaddq_u32(previous, sum);
    sum = vpadalq_u16(sum, vpaddlq_u8(value));
    columns6 = vaddw_u8(columns6, vget_low_u8(value));
    columns7 = vaddw_u8(columns7, vget_high_u8(value));
  }
  auto const add_weighted = [](uint32x4_t total, uint16x8_t columns, std::uint16_t const *column_weights) {
    uint16x8_t const weight = vld1q_u16(column_weights);
    total = vmlal_u16(total, vget_low_u16(columns), vget_low_u16(weight));
    return vmlal_u16(total, vget_high_u16(columns), vget_high_u16(weight));
  };
  uint32x4_t weighted = vmull_u16(vget_low_u16(columns0), vget_low_u16(vld1q_u16(weights.data())));
  weighted = vmlal_u16(weighted, vget_high_u16(columns0), vget_high_u16(vld1q_u16(weights.data())));
  weighted = add_weighted(weighted, columns1, weights.data() + 8);
  weighted = add_weighted(weighted, columns2, weights.data() + 16);
  weighted = add_weighted(weighted, columns3, weights.data() + 24);
  weighted = add_weighted(weighted, columns4, weights.data() + 32);
  weighted = add_weighted(weighted, columns5, weights.data() + 40);
  weighted = add_weighted(weighted, columns6, weights.data() + 48);
  weighted = add_weighted(weighted, columns7, weights.data() + 56);
  return {.sum = lane_total_32<Isa>(sum), .weighted = (block_size * lane_total_32<Isa>(previous)) + lane_total_32<Isa>(weighted)};
}
#endif

// As for bytes, with 8 values per block and 32-bit column sums.
template <isa Isa>
  requires(neon_target<Isa>)
chunk_sums block_kernel<16, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  static constexpr std::array<std::uint32_t, 8> weights{8, 7, 6, 5, 4, 3, 2, 1};
  auto const *position = reinterpret_cast<std::uint8_t const *>(data);
  uint32x4_t sum = vdupq_n_u32(0);
  uint32x4_t previous = sum;
  uint32x4_t low_columns = sum;
  uint32x4_t high_columns = sum;
  for(; blocks != 0; --blocks, position += block_size) {
    uint16x8_t const value = vreinterpretq_u16_u8(vld1q_u8(position));
    previous = vaddq_u32(previous, sum);
    sum = vpadalq_u16(sum, value);
    low_columns = vaddw_u16(low_columns, vget_low_u16(value));
    high_columns = vaddw_u16(high_columns, vget_high_u16(value));
  }
  uint32x4_t const low_weights = vld1q_u32(weights.data());
  uint32x4_t const high_weights = vld1q_u32(weights.data() + 4);
  uint64x2_t weighted = vmull_u32(vget_low_u32(low_columns), vget_low_u32(low_weights));
  weighted = vmlal_u32(weighted, vget_high_u32(low_columns), vget_high_u32(low_weights));
  weighted = vmlal_u32(weighted, vget_low_u32(high_columns), vget_low_u32(high_weights));
  weighted = vmlal_u32(weighted, vget_high_u32(high_columns), vget_high_u32(high_weights));
  return {.sum = lane_total_32<Isa>(sum), .weighted = ((block_size / 2) * lane_total_32<Isa>(previous)) + lane_total_64<Isa>(weighted)};
}

// As for bytes, with 4 values per block and 64-bit lanes.
template <isa Isa>
  requires(neon_target<Isa>)
chunk_sums block_kernel<32, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  auto const *position = reinterpret_cast<std::uint8_t const *>(data);
  uint64x2_t sum = vdupq_n_u64(0);
  uint64x2_t previous = sum;
  uint64x2_t low_columns = sum;
  uint64x2_t high_columns = sum;
  for(; blocks != 0; --blocks, position += block_size) {
    uint32x4_t const value = vreinterpretq_u32_u8(vld1q_u8(position));
    previous = vaddq_u64(previous, sum);
    sum = vpadalq_u32(sum, value);
    low_columns = vaddw_u32(low_columns, vget_low_u32(value));
    high_columns = vaddw_u32(high_columns, vget_high_u32(value));
  }
  std::uint64_t const weighted = (4 * vgetq_lane_u64(low_columns, 0)) + (3 * vgetq_lane_u64(low_columns, 1)) + (2 * vgetq_lane_u64(high_columns, 0)) +
                                 vgetq_lane_u64(high_columns, 1);
  return {.sum = lane_total_64<Isa>(sum), .weighted = ((block_size / 4) * lane_total_64<Isa>(previous)) + weighted};
}

} // namespace checksum::fletcher_detail

#endif

#if CHECKSUM_PRESET_HAS(DSP)

#include <arm_acle.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace checksum::fletcher_detail {

// The argument type of __uxtb16: uint8x4_t in ACLE and GCC, int8x4_t in Clang.
#if defined(__clang__)
using uxtb16_argument = int8x4_t;
#else
using uxtb16_argument = uint8x4_t;
#endif

template <isa Isa>
inline constexpr bool dsp_kernel =
    std::endian::native == std::endian::little && isa_detail::includes(Isa, isa::thumb2_dsp) && !isa_detail::includes(Isa, isa::thumb2_neon);

// The little-endian word at data.
template <isa Isa> std::uint32_t load_word(std::byte const *data) noexcept;

// The weights pick bytes by their position in a little-endian word.
template <isa Isa>
  requires(dsp_kernel<Isa>)
struct block_kernel<8, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 8;
  // The weighted sum of n bytes, at most 255 * n(n + 1) / 2, stays within 32 bits.
  static constexpr std::size_t max_blocks = 700;
  static_assert(std::uint64_t{255} * (block_size * max_blocks) * ((block_size * max_blocks) + 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // Faster than the portable loop from 64 bytes on a Cortex-M4 (nRF52840, STM32L4A6).
  static constexpr std::size_t minimum_size = 64;
  // Word loads from an address that is not a multiple of 4 make the Cortex-M4 loop 15-25 % slower.
  static constexpr std::size_t alignment = 4;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

} // namespace checksum::fletcher_detail

namespace checksum::fletcher_detail {

template <isa Isa> std::uint32_t load_word(std::byte const *data) noexcept {
  std::uint32_t word = 0;
  std::memcpy(&word, data, sizeof(word));
  return word;
}

// Per 8 bytes: weighted += 8 * sum, usada8 adds the bytes to sum, and smlad adds bytes 0 and 2 (uxtb16) and 1 and 3 (after a
// rotation) of each word with their weights 8 .. 1. Not inlined: inside the chunk loop, GCC reloaded two weights in every iteration.
template <isa Isa>
  requires(dsp_kernel<Isa>)
CHECKSUM_NOINLINE chunk_sums block_kernel<8, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  std::uint32_t sum = 0;
  std::uint32_t weighted = 0;
  for(; blocks != 0; --blocks, data += block_size) {
    std::uint32_t const first = load_word<Isa>(data);
    std::uint32_t const second = load_word<Isa>(data + 4);
    weighted += 8 * sum;
    sum = __usada8(first, 0, sum);
    sum = __usada8(second, 0, sum);
    auto total = static_cast<std::int32_t>(weighted);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(static_cast<uxtb16_argument>(first))), 0x0006'0008, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(static_cast<uxtb16_argument>(__ror(first, 8)))), 0x0005'0007, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(static_cast<uxtb16_argument>(second))), 0x0002'0004, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(static_cast<uxtb16_argument>(__ror(second, 8)))), 0x0001'0003, total);
    weighted = static_cast<std::uint32_t>(total);
  }
  return {.sum = sum, .weighted = weighted};
}

} // namespace checksum::fletcher_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_FLETCHER_HPP
