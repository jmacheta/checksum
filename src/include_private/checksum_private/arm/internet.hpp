#ifndef CHECKSUM_PRIVATE_ARM_INTERNET_HPP
#define CHECKSUM_PRIVATE_ARM_INTERNET_HPP

// Arm kernels of the Internet checksum: NEON pairwise add-accumulate on little-endian AArch64 for every preset and on
// little-endian 32-bit Arm from thumb2_neon on, else an ldm + adcs chain on 32-bit Arm from thumb2 on. The thumb2_neon and
// thumb2_crc sources define CHECKSUM_PRESET_ARM_NEON to enable the NEON section. Included only by internet_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/arm/target.hpp>

#if defined(CHECKSUM_ARM64) || (defined(__arm__) && defined(CHECKSUM_PRESET_ARM_NEON))
#include <arm_neon.h>
#endif

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace checksum::internet_detail {

using arm_detail::arm64_target;

// NEON lanes are read as little-endian words; the presets of 32-bit Arm compile Thumb-2 code.
template <isa Isa>
inline constexpr bool neon_kernel = std::endian::native == std::endian::little && (arm64_target || isa_detail::includes(Isa, isa::thumb2_neon));
template <isa Isa> inline constexpr bool ldm_kernel = !arm64_target && !neon_kernel<Isa> && isa_detail::includes(Isa, isa::thumb2);

template <isa Isa> inline constexpr bool block_sum_available = neon_kernel<Isa> || ldm_kernel<Isa>;

// The NEON kernel is above where it overtakes the portable loop on a Cortex-A72, about 370 bytes in AArch64 and 110 in
// AArch32: the 64-bit loop is fast, the 32-bit one is not. The ldm kernel is faster from 192 bytes on a Cortex-M4.
template <isa Isa>
inline constexpr std::size_t block_sum_minimum_size = block_sum_available<Isa> ? (arm64_target ? 512 : 192) : std::numeric_limits<std::size_t>::max();

inline constexpr std::size_t neon_block_size = 64;

// Each 64-bit lane takes two 32-bit words per block: passes of 2^26 blocks keep the lanes and their merged sum below 2^61.
inline constexpr std::size_t neon_pass_blocks = std::size_t{1} << 26U;

inline constexpr std::size_t ldm_block_size = 32;

// 64-byte blocks of 32-bit words, added pairwise into four vectors of 64-bit lanes (uadalp). Defined where NEON is enabled.
template <isa Isa> block_total neon_sum(std::span<std::byte const> data) noexcept;

// 32-byte blocks from an even address (aligned to 4 bytes first): two ldm of four words, eight adcs. Thumb-2 only.
template <isa Isa> block_total ldm_sum(std::span<std::byte const> data) noexcept;

// A byte at an even offset: the first byte of a native 16-bit word.
template <isa Isa> std::uint64_t first_byte_value(std::byte value) noexcept;

#if defined(CHECKSUM_ARM64) || (defined(__arm__) && defined(CHECKSUM_PRESET_ARM_NEON))

template <isa Isa> block_total neon_sum(std::span<std::byte const> data) noexcept {
  std::size_t const message_size = data.size();
  std::uint64_t total = 0;
  while(data.size() >= neon_block_size) {
    std::size_t const blocks = std::min(data.size() / neon_block_size, neon_pass_blocks);
    uint64x2_t sum0 = vdupq_n_u64(0);
    uint64x2_t sum1 = sum0;
    uint64x2_t sum2 = sum0;
    uint64x2_t sum3 = sum0;
    auto const *position = reinterpret_cast<std::uint8_t const *>(data.data());
    for(std::size_t block = 0; block < blocks; ++block, position += neon_block_size) {
      sum0 = vpadalq_u32(sum0, vreinterpretq_u32_u8(vld1q_u8(position)));
      sum1 = vpadalq_u32(sum1, vreinterpretq_u32_u8(vld1q_u8(position + 16)));
      sum2 = vpadalq_u32(sum2, vreinterpretq_u32_u8(vld1q_u8(position + 32)));
      sum3 = vpadalq_u32(sum3, vreinterpretq_u32_u8(vld1q_u8(position + 48)));
    }
    data = data.subspan(blocks * neon_block_size);
    uint64x2_t const sum = vaddq_u64(vaddq_u64(sum0, sum1), vaddq_u64(sum2, sum3));
    total += fold(vgetq_lane_u64(sum, 0));
    total += fold(vgetq_lane_u64(sum, 1));
  }
  return {.sum = total, .size = message_size - data.size()};
}

#endif

#if defined(__arm__)

template <isa Isa> block_total ldm_sum(std::span<std::byte const> data) noexcept {
  std::size_t const message_size = data.size();
  std::byte const *position = data.data();
  std::uint64_t total = 0;
  if((reinterpret_cast<std::uintptr_t>(position) & 2U) != 0) {
    std::uint16_t half = 0;
    std::memcpy(&half, position, sizeof(half));
    total = half;
    position += sizeof(half);
  }
  std::size_t const blocks = (data.size() - static_cast<std::size_t>(position - data.data())) / ldm_block_size;
  if(blocks != 0) {
    // end becomes the last carry once the loop is done.
    auto end = reinterpret_cast<std::uintptr_t>(position + (blocks * ldm_block_size));
    std::uint32_t sum = 0;
    asm("adds %[sum], %[sum], #0\n"
        ".p2align 2\n"
        "1: ldmia %[position]!, {r2, r3, r4, r5}\n"
        "adcs %[sum], %[sum], r2\n"
        "adcs %[sum], %[sum], r3\n"
        "adcs %[sum], %[sum], r4\n"
        "adcs %[sum], %[sum], r5\n"
        "ldmia %[position]!, {r2, r3, r4, r5}\n"
        "adcs %[sum], %[sum], r2\n"
        "adcs %[sum], %[sum], r3\n"
        "adcs %[sum], %[sum], r4\n"
        "adcs %[sum], %[sum], r5\n"
        "teq %[position], %[end]\n"
        "bne 1b\n"
        "mov %[end], #0\n"
        "adc %[end], %[end], #0\n"
        : [sum] "+r"(sum), [position] "+r"(position), [end] "+r"(end)
        :
        : "r2", "r3", "r4", "r5", "cc", "memory");
    total += std::uint64_t{sum} + end;
  }
  data = data.subspan(static_cast<std::size_t>(position - data.data()));
  return {.sum = total, .size = message_size - data.size()};
}

#endif

template <isa Isa> std::uint64_t first_byte_value(std::byte value) noexcept {
  return std::to_integer<std::uint64_t>(value) << (std::endian::native == std::endian::little ? 0U : 8U);
}

// The ldm kernel from an odd address: the words that start at the next byte are this message's shifted by one byte, so
// their sum is byte-swapped. One more byte after them keeps the size taken even.
template <isa Isa> block_total block_sum(std::span<std::byte const> data) noexcept {
  if constexpr(neon_kernel<Isa>) {
    return neon_sum<Isa>(data);
  } else {
    if((reinterpret_cast<std::uintptr_t>(data.data()) & 1U) == 0) {
      return ldm_sum<Isa>(data);
    }
    block_total shifted = ldm_sum<Isa>(data.subspan(1));
    if(shifted.size + 1 < data.size()) {
      shifted.sum += first_byte_value<Isa>(data[shifted.size + 1]);
      ++shifted.size;
    }
    return {.sum = first_byte_value<Isa>(data[0]) + std::byteswap(fold(shifted.sum)), .size = shifted.size + 1};
  }
}

} // namespace checksum::internet_detail

#endif // CHECKSUM_PRIVATE_ARM_INTERNET_HPP
