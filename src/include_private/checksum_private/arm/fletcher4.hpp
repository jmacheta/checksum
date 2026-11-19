#ifndef CHECKSUM_PRIVATE_ARM_FLETCHER4_HPP
#define CHECKSUM_PRIVATE_ARM_FLETCHER4_HPP

// NEON kernel of the fletcher4 lanes on little-endian AArch64 for every preset and on little-endian 32-bit Arm from
// thumb2_neon: the four lanes of each sum in two 128-bit vectors. The preset sources define CHECKSUM_PRESET_ARM_NEON to
// enable the 32-bit Arm section. Included only by fletcher4_arch.hpp.

#include <checksum/isa.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(__aarch64__) || defined(_M_ARM64) || (defined(__arm__) && defined(CHECKSUM_PRESET_ARM_NEON))

#include <arm_neon.h>

namespace checksum::fletcher4_detail {

#if defined(__aarch64__) || defined(_M_ARM64)
inline constexpr bool arm64_target = true;
#else
inline constexpr bool arm64_target = false;
#endif

template <isa Isa>
inline constexpr bool lane_kernel_available =
    std::endian::native == std::endian::little && (arm64_target || isa_detail::includes(Isa, isa::thumb2_neon));

template <isa Isa>
inline constexpr std::size_t lane_kernel_minimum_size = lane_kernel_available<Isa> ? 192 : std::numeric_limits<std::size_t>::max();

// Lanes 0 and 1 in the low vectors, 2 and 3 in the high ones.
template <isa Isa> lane_sums lane_kernel(std::byte const *data, std::size_t groups) noexcept {
  uint64x2_t sum1_low = vdupq_n_u64(0);
  uint64x2_t sum1_high = vdupq_n_u64(0);
  uint64x2_t sum2_low = vdupq_n_u64(0);
  uint64x2_t sum2_high = vdupq_n_u64(0);
  uint64x2_t sum3_low = vdupq_n_u64(0);
  uint64x2_t sum3_high = vdupq_n_u64(0);
  uint64x2_t sum4_low = vdupq_n_u64(0);
  uint64x2_t sum4_high = vdupq_n_u64(0);
  for(; groups != 0; --groups, data += lane_count * word_size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
    uint32x4_t const words = vreinterpretq_u32_u8(vld1q_u8(reinterpret_cast<std::uint8_t const *>(data)));
    sum1_low = vaddw_u32(sum1_low, vget_low_u32(words));
    sum1_high = vaddw_u32(sum1_high, vget_high_u32(words));
    sum2_low = vaddq_u64(sum2_low, sum1_low);
    sum2_high = vaddq_u64(sum2_high, sum1_high);
    sum3_low = vaddq_u64(sum3_low, sum2_low);
    sum3_high = vaddq_u64(sum3_high, sum2_high);
    sum4_low = vaddq_u64(sum4_low, sum3_low);
    sum4_high = vaddq_u64(sum4_high, sum3_high);
  }
  lane_sums lanes{};
  vst1q_u64(lanes[0].data(), sum1_low);
  vst1q_u64(lanes[0].data() + 2, sum1_high);
  vst1q_u64(lanes[1].data(), sum2_low);
  vst1q_u64(lanes[1].data() + 2, sum2_high);
  vst1q_u64(lanes[2].data(), sum3_low);
  vst1q_u64(lanes[2].data() + 2, sum3_high);
  vst1q_u64(lanes[3].data(), sum4_low);
  vst1q_u64(lanes[3].data() + 2, sum4_high);
  return lanes;
}

} // namespace checksum::fletcher4_detail

#else

#include <checksum_private/generic/fletcher4.hpp>

#endif

#endif // CHECKSUM_PRIVATE_ARM_FLETCHER4_HPP
