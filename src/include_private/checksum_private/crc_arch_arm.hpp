#ifndef CHECKSUM_PRIVATE_CRC_ARCH_ARM_HPP
#define CHECKSUM_PRIVATE_CRC_ARCH_ARM_HPP

// Arm kernels: the CRC32/CRC32C instructions (AArch64 and AArch32 with +crc) and PMULL folding (little-endian AArch64
// with +crypto; big-endian NEON lane order differs between compilers). Included only by crc_arch.hpp.

#include <arm_acle.h>
#if defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_AES) && !defined(__AARCH64EB__)
#include <arm_neon.h>
#endif

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::crc_detail {

#if defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_AES) && !defined(__AARCH64EB__)
inline constexpr bool folding_available = true;
#else
inline constexpr bool folding_available = false;
#endif

#if defined(__ARM_FEATURE_CRC32)
template <std::uint32_t Polynomial> inline constexpr bool crc32_instructions_available = true;
#else
template <std::uint32_t Polynomial> inline constexpr bool crc32_instructions_available = false;
#endif

// CRC32 instructions alone, never folding: one CRC32X stream takes 8 cycles per 64 bytes on a Cortex-A72, while llvm-mca
// puts the PMULL loop at 10 (A72, A76), 24-33 (A510, A53, A55) and 6-9 (Neoverse N1/V1/V2, Apple M1) plus a reduction.
inline constexpr std::size_t crc32_folding_minimum_size = std::numeric_limits<std::size_t>::max();

inline constexpr std::size_t wide_folding_minimum_size = std::numeric_limits<std::size_t>::max();

#if defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_AES) && !defined(__AARCH64EB__)
// Fold kernel of fold_blocks_with(): 128-bit NEON vectors, lane 0 low.
struct pmull_kernel {
  using vector = uint64x2_t;

  static constexpr bool wide = false;

  // Byte 0 lowest if reflected, byte-reversed otherwise.
  template <bool Reflected> static vector load(std::span<std::byte const> data) noexcept;

  static vector pair(folding_pair const &constants) noexcept;

  // The register goes into lane 0 if reflected, lane 1 otherwise.
  template <bool Reflected> static vector first_block(std::span<std::byte const> data, std::uint64_t remainder) noexcept;

  // Lane-wise products with the constants, plus next; one EOR3 with the SHA3 extension.
  static vector fold(vector accumulator, vector constants, vector next) noexcept;

  // In message order, the accumulator's first size bytes (zero-extended in front) fold over one block onto its other
  // bytes followed by the last size message bytes.
  template <bool Reflected> static vector partial(vector accumulator, vector by_one, std::span<std::byte const> last, std::size_t size) noexcept;

  // Carry-less 64 x 64-bit product, low 64 bits in lane 0.
  static vector multiply(std::uint64_t first, std::uint64_t second) noexcept;

  // A * x^64 mod P: the high half folded over 128 bits, then a Barrett reduction. Reflected products carry an extra
  // factor x (see make_folding_constants()), hence the one-bit shifts.
  template <bool Reflected> static std::uint64_t reduce(vector accumulator, folding_constants const &constants) noexcept;
};
#endif

#if defined(__ARM_FEATURE_CRC32)
template <std::uint32_t Polynomial> [[gnu::always_inline]] inline std::uint32_t crc32_word(std::uint32_t remainder, std::uint64_t word) noexcept {
  static_assert(Polynomial == crc32_reflected_polynomial || Polynomial == crc32c_reflected_polynomial);
  return Polynomial == crc32c_reflected_polynomial ? __crc32cd(remainder, word) : __crc32d(remainder, word);
}

template <std::uint32_t Polynomial>
[[gnu::always_inline]] inline std::uint32_t crc32_instructions(std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  static_assert(Polynomial == crc32_reflected_polynomial || Polynomial == crc32c_reflected_polynomial);
  constexpr bool castagnoli = Polynomial == crc32c_reflected_polynomial;
  constexpr std::size_t word = 8;
  auto const update_word = [&](std::span<std::byte const> bytes) { remainder = crc32_word<Polynomial>(remainder, load_little_endian<word>(bytes)); };
  // Four words per iteration, then 2 and 1: one word per iteration is loop bound (~2 cycles per word instead of 1 on a
  // Cortex-A72). A loop running exactly twice costs a branch miss there, which this moves from 32-47 to 64-95 bytes.
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
    auto const value = static_cast<std::uint32_t>(load_little_endian<4>(data));
    remainder = castagnoli ? __crc32cw(remainder, value) : __crc32w(remainder, value);
    data = data.subspan(4);
  }
  if(data.size() >= 2) {
    auto const value = static_cast<std::uint16_t>(load_little_endian<2>(data));
    remainder = castagnoli ? __crc32ch(remainder, value) : __crc32h(remainder, value);
    data = data.subspan(2);
  }
  if(!data.empty()) {
    auto const value = std::to_integer<std::uint8_t>(data[0]);
    remainder = castagnoli ? __crc32cb(remainder, value) : __crc32b(remainder, value);
  }
  return remainder;
}
#endif

#if defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_AES) && !defined(__AARCH64EB__)
template <bool Reflected> [[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::load(std::span<std::byte const> data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  uint8x16_t const block = vld1q_u8(reinterpret_cast<std::uint8_t const *>(data.data()));
  if constexpr(Reflected) {
    return vreinterpretq_u64_u8(block);
  } else {
    uint8x16_t const swapped = vrev64q_u8(block);
    return vreinterpretq_u64_u8(vextq_u8(swapped, swapped, 8));
  }
}

[[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::pair(folding_pair const &constants) noexcept { return vld1q_u64(constants.data()); }

template <bool Reflected>
[[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::first_block(std::span<std::byte const> data, std::uint64_t remainder) noexcept {
  vector const state = Reflected ? vcombine_u64(vcreate_u64(remainder), vcreate_u64(0)) : vcombine_u64(vcreate_u64(0), vcreate_u64(remainder));
  return veorq_u64(load<Reflected>(data), state);
}

[[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::fold(vector accumulator, vector constants, vector next) noexcept {
  poly128_t const low = vmull_p64(vgetq_lane_u64(accumulator, 0), vgetq_lane_u64(constants, 0));
  poly128_t const high = vmull_high_p64(vreinterpretq_p64_u64(accumulator), vreinterpretq_p64_u64(constants));
#if defined(__ARM_FEATURE_SHA3)
  // Compilers do not fuse the two XORs themselves (1.5x on Neoverse V2 per llvm-mca).
  return veor3q_u64(vreinterpretq_u64_p128(low), vreinterpretq_u64_p128(high), next);
#else
  return veorq_u64(veorq_u64(vreinterpretq_u64_p128(low), vreinterpretq_u64_p128(high)), next);
#endif
}

template <bool Reflected>
[[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::partial(vector accumulator, vector by_one, std::span<std::byte const> last,
                                                                         std::size_t size) noexcept {
  auto const reverse = [](uint8x16_t bytes) {
    uint8x16_t const swapped = vrev64q_u8(bytes);
    return vextq_u8(swapped, swapped, 8);
  };
  uint8x16_t const message = Reflected ? vreinterpretq_u8_u64(accumulator) : reverse(vreinterpretq_u8_u64(accumulator));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  uint8x16_t const tail = vld1q_u8(reinterpret_cast<std::uint8_t const *>(last.data()));
  uint8x16_t const to_end = vld1q_u8(block_shift_indices.data() + size);
  uint8x16_t const to_start = vld1q_u8(block_shift_indices.data() + folding_block_size + size);
  uint8x16_t const first = vqtbl1q_u8(message, to_end);
  uint8x16_t const second = vbslq_u8(vcgeq_u8(to_start, vdupq_n_u8(folding_block_size)), tail, vqtbl1q_u8(message, to_start));
  if constexpr(Reflected) {
    return fold(vreinterpretq_u64_u8(first), by_one, vreinterpretq_u64_u8(second));
  } else {
    return fold(vreinterpretq_u64_u8(reverse(first)), by_one, vreinterpretq_u64_u8(reverse(second)));
  }
}

[[gnu::always_inline]] inline pmull_kernel::vector pmull_kernel::multiply(std::uint64_t first, std::uint64_t second) noexcept {
  return vreinterpretq_u64_p128(vmull_p64(first, second));
}

template <bool Reflected>
[[gnu::always_inline]] inline std::uint64_t pmull_kernel::reduce(vector accumulator, folding_constants const &constants) noexcept {
  constexpr unsigned top_bit = 63;
  std::uint64_t const low = vgetq_lane_u64(accumulator, 0);
  std::uint64_t const high = vgetq_lane_u64(accumulator, 1);
  if constexpr(Reflected) {
    vector const folded = multiply(low, constants.by_one[1]);
    std::uint64_t const value_high = vgetq_lane_u64(folded, 0) ^ high;
    std::uint64_t const quotient = value_high ^ (vgetq_lane_u64(multiply(value_high, constants.quotient), 0) << 1U);
    vector const product = multiply(quotient, constants.polynomial);
    return vgetq_lane_u64(folded, 1) ^ (vgetq_lane_u64(product, 0) >> top_bit) ^ (vgetq_lane_u64(product, 1) << 1U);
  } else {
    vector const folded = multiply(high, constants.by_one[0]);
    std::uint64_t const value_high = vgetq_lane_u64(folded, 1) ^ low;
    std::uint64_t const quotient = value_high ^ vgetq_lane_u64(multiply(value_high, constants.quotient), 1);
    return vgetq_lane_u64(folded, 0) ^ vgetq_lane_u64(multiply(quotient, constants.polynomial), 0);
  }
}

template <bool Reflected>
[[gnu::always_inline]] inline std::uint64_t fold_blocks(folding_constants const &constants, std::uint64_t remainder,
                                                        std::span<std::byte const> data) noexcept {
  return pmull_kernel::reduce<Reflected>(fold_blocks_with<Reflected, pmull_kernel>(constants, remainder, data), constants);
}
#endif

} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_CRC_ARCH_ARM_HPP
