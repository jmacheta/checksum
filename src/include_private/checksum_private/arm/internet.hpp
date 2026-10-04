#ifndef CHECKSUM_PRIVATE_ARM_INTERNET_HPP
#define CHECKSUM_PRIVATE_ARM_INTERNET_HPP

// Arm kernels of the Internet checksum: NEON pairwise add-accumulate on little-endian AArch64 and AArch32, else an ldm +
// adcs chain on 32-bit Arm (Thumb-2 or Arm state, e.g. Cortex-M3/M4/M7/M33). Included only by internet_arch.hpp.

#if !(defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) &&                                                                           \
    !(defined(__arm__) && !defined(__ARM_NEON) && (!defined(__thumb__) || defined(__thumb2__)))
#include <checksum_private/generic/internet.hpp>
#else

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace checksum::internet_detail {

inline constexpr bool block_sum_available = true;

#if defined(__ARM_NEON)

// Above where the kernel overtakes the portable loop on a Cortex-A72, about 370 bytes in AArch64 and 110 in AArch32:
// the 64-bit loop is fast, the 32-bit one is not.
#if defined(__aarch64__)
inline constexpr std::size_t block_sum_minimum_size = 512;
#else
inline constexpr std::size_t block_sum_minimum_size = 192;
#endif

inline constexpr std::size_t neon_block_size = 64;

// Each 64-bit lane takes two 32-bit words per block: passes of 2^26 blocks keep the lanes and their merged sum below 2^61.
inline constexpr std::size_t neon_pass_blocks = std::size_t{1} << 26U;

// 64-byte blocks of 32-bit words, added pairwise into four vectors of 64-bit lanes (uadalp).
inline block_total block_sum(std::span<std::byte const> data) noexcept {
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

#else

// From 192 bytes the kernel is faster than the portable loop on a Cortex-M4, from any start address.
inline constexpr std::size_t block_sum_minimum_size = 192;

inline constexpr std::size_t ldm_block_size = 32;

// 32-byte blocks from an even address (aligned to 4 bytes first): two ldm of four words, eight adcs.
inline block_total ldm_sum(std::span<std::byte const> data) noexcept;

// A byte at an even offset: the first byte of a native 16-bit word.
inline std::uint64_t first_byte_value(std::byte value) noexcept;

inline block_total ldm_sum(std::span<std::byte const> data) noexcept {
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

inline std::uint64_t first_byte_value(std::byte value) noexcept {
  return std::to_integer<std::uint64_t>(value) << (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ? 0U : 8U);
}

// From an odd address, the words that start at the next byte are this message's shifted by one byte, so their sum is
// byte-swapped. One more byte after them keeps the size taken even.
inline block_total block_sum(std::span<std::byte const> data) noexcept {
  if((reinterpret_cast<std::uintptr_t>(data.data()) & 1U) == 0) {
    return ldm_sum(data);
  }
  block_total shifted = ldm_sum(data.subspan(1));
  if(shifted.size + 1 < data.size()) {
    shifted.sum += first_byte_value(data[shifted.size + 1]);
    ++shifted.size;
  }
  return {.sum = first_byte_value(data[0]) + std::byteswap(fold(shifted.sum)), .size = shifted.size + 1};
}

#endif

} // namespace checksum::internet_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_INTERNET_HPP
