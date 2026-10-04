#ifndef CHECKSUM_PRIVATE_ARM_FLETCHER_HPP
#define CHECKSUM_PRIVATE_ARM_FLETCHER_HPP

// Arm kernels of the Fletcher checksums and Adler-32, included only by fletcher_arch.hpp: NEON on little-endian AArch64 and
// AArch32, else the DSP instructions for bytes on little-endian M-profile cores (e.g. Cortex-M4/M7/M33). Only for tests in
// QEMU user mode, the cross-arm-portable preset defines CHECKSUM_TEST_ARM_DSP to use the DSP kernel on an A-profile core.

#include <checksum_private/generic/fletcher.hpp>

#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#include <arm_neon.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::fletcher_detail {

// Sums of the lanes, in 64 bits.
inline std::uint64_t lane_total(uint32x4_t value) noexcept;
inline std::uint64_t lane_total(uint64x2_t value) noexcept;

template <> struct kernel<8> {
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

template <> struct kernel<16> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // A 32-bit lane of the previous sums gains at most 2 * 65535 * j at block j.
  static constexpr std::size_t max_blocks = 128;
  static_assert(std::uint64_t{2 * 65'535} * max_blocks * (max_blocks - 1) / 2 <= std::numeric_limits<std::uint32_t>::max());
  // Slower than the portable loop at 64 bytes on a Cortex-A72 (Raspberry Pi 4), in AArch64 and AArch32.
  static constexpr std::size_t minimum_size = 128;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

template <> struct kernel<32> {
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

inline std::uint64_t lane_total(uint64x2_t value) noexcept { return vgetq_lane_u64(value, 0) + vgetq_lane_u64(value, 1); }

inline std::uint64_t lane_total(uint32x4_t value) noexcept { return lane_total(vpaddlq_u32(value)); }

// Per block: previous += sum, so that previous ends as the sum over blocks of the byte sums before each block. Column sums of
// the 16 bytes get their weights 16 .. 1 at the end of the chunk.
inline chunk_sums kernel<8>::sum(std::byte const *data, std::size_t blocks) noexcept {
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
  return {.sum = lane_total(sum), .weighted = (block_size * lane_total(previous)) + lane_total(weighted)};
}

// As for bytes, with 8 values per block and 32-bit column sums.
inline chunk_sums kernel<16>::sum(std::byte const *data, std::size_t blocks) noexcept {
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
  return {.sum = lane_total(sum), .weighted = ((block_size / 2) * lane_total(previous)) + lane_total(weighted)};
}

// As for bytes, with 4 values per block and 64-bit lanes.
inline chunk_sums kernel<32>::sum(std::byte const *data, std::size_t blocks) noexcept {
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
  return {.sum = lane_total(sum), .weighted = ((block_size / 4) * lane_total(previous)) + weighted};
}

} // namespace checksum::fletcher_detail

#elif defined(__arm__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && defined(__ARM_FEATURE_DSP) && defined(__ARM_FEATURE_SIMD32) &&                \
    defined(__ARM_FEATURE_UNALIGNED) && ((defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M') || defined(CHECKSUM_TEST_ARM_DSP))

#include <arm_acle.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace checksum::fletcher_detail {

// The little-endian word at data.
inline std::uint32_t load_word(std::byte const *data) noexcept;

template <> struct kernel<8> {
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

inline std::uint32_t load_word(std::byte const *data) noexcept {
  std::uint32_t word = 0;
  std::memcpy(&word, data, sizeof(word));
  return word;
}

// Per 8 bytes: weighted += 8 * sum, usada8 adds the bytes to sum, and smlad adds bytes 0 and 2 (uxtb16) and 1 and 3 (after a
// rotation) of each word with their weights 8 .. 1. Not inlined: inside the chunk loop, GCC reloaded two weights in every iteration.
[[gnu::noinline]] inline chunk_sums kernel<8>::sum(std::byte const *data, std::size_t blocks) noexcept {
  std::uint32_t sum = 0;
  std::uint32_t weighted = 0;
  for(; blocks != 0; --blocks, data += block_size) {
    std::uint32_t const first = load_word(data);
    std::uint32_t const second = load_word(data + 4);
    weighted += 8 * sum;
    sum = __usada8(first, 0, sum);
    sum = __usada8(second, 0, sum);
    auto total = static_cast<std::int32_t>(weighted);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(first)), 0x0006'0008, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(__ror(first, 8))), 0x0005'0007, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(second)), 0x0002'0004, total);
    total = __smlad(static_cast<std::int32_t>(__uxtb16(__ror(second, 8))), 0x0001'0003, total);
    weighted = static_cast<std::uint32_t>(total);
  }
  return {.sum = sum, .weighted = weighted};
}

} // namespace checksum::fletcher_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_FLETCHER_HPP
