#ifndef CHECKSUM_PRIVATE_ARM_XXH3_HPP
#define CHECKSUM_PRIVATE_ARM_XXH3_HPP

// NEON kernel of the XXH3 stripe loop on little-endian AArch64 and AArch32. Included only by xxh3_arch.hpp.

#if !(defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#include <checksum_private/generic/xxh3.hpp>
#else

#include <arm_neon.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace checksum::xxh3_detail {

inline constexpr bool stripe_kernel_available = true;

#if defined(__aarch64__)

// On a Cortex-A72 four lanes in NEON and four scalar ones beat both all eight in NEON and the portable loop.
inline constexpr std::size_t vector_count = 2;

// With GCC on a Cortex-A72 the kernel beats the portable loop from 512-byte messages, 448 bytes of whole stripes (3.31 against
// 3.25 GiB/s; 3.89 against 3.67 at 1500 bytes, 4.24 against 3.93 at 4096, 3.74 against 3.44 at 1 MiB), but not at 256 (2.52 against 2.57).
inline constexpr std::size_t stripe_kernel_minimum_size = 448;

#else

inline constexpr std::size_t vector_count = 4;

// With GCC on a Cortex-A72 in AArch32 the kernel beats the portable loop 1.4 to 1.55 times from 256 bytes, the shortest size measured.
inline constexpr std::size_t stripe_kernel_minimum_size = 0;

#endif

// The first lanes in 128-bit vectors of two, on AArch64 the rest in scalars. An empty scalar array would keep GCC on AArch32
// from holding the lanes in registers.
struct stripe_kernel {
  struct lanes {
    std::array<uint64x2_t, vector_count> vectors;
#if defined(__aarch64__)
    std::array<std::uint64_t, 8 - (2 * vector_count)> scalars;
#endif
  };
  static lanes load(accumulator_array const &accumulators) noexcept;
  static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  static void scramble(lanes &values, std::byte const *secret) noexcept;
};

// The 16 bytes at data as two little-endian 64-bit lanes.
inline uint64x2_t load_vector(std::byte const *data) noexcept;

inline uint64x2_t load_vector(std::byte const *data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  return vreinterpretq_u64_u8(vld1q_u8(reinterpret_cast<std::uint8_t const *>(data)));
}

inline stripe_kernel::lanes stripe_kernel::load(accumulator_array const &accumulators) noexcept {
  lanes values{};
  for(std::size_t index = 0; index < vector_count; ++index) {
    values.vectors[index] = vld1q_u64(accumulators.data() + (2 * index));
  }
#if defined(__aarch64__)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    values.scalars[index] = accumulators[(2 * vector_count) + index];
  }
#endif
  return values;
}

inline void stripe_kernel::store(accumulator_array &accumulators, lanes const &values) noexcept {
  for(std::size_t index = 0; index < vector_count; ++index) {
    vst1q_u64(accumulators.data() + (2 * index), values.vectors[index]);
  }
#if defined(__aarch64__)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    accumulators[(2 * vector_count) + index] = values.scalars[index];
  }
#endif
}

// Two vectors at a time: one unzip gives the low and the high 32-bit halves of data ^ secret for four lanes.
inline void stripe_kernel::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  for(std::size_t index = 0; index < vector_count; index += 2) {
    uint64x2_t const first = load_vector(stripe + (16 * index));
    uint64x2_t const second = load_vector(stripe + (16 * index) + 16);
    uint64x2_t const first_key = veorq_u64(first, load_vector(secret + (16 * index)));
    uint64x2_t const second_key = veorq_u64(second, load_vector(secret + (16 * index) + 16));
    uint32x4x2_t const halves = vuzpq_u32(vreinterpretq_u32_u64(first_key), vreinterpretq_u32_u64(second_key));
    uint64x2_t const first_sum = vmlal_u32(vextq_u64(first, first, 1), vget_low_u32(halves.val[0]), vget_low_u32(halves.val[1]));
#if defined(__aarch64__)
    uint64x2_t const second_sum = vmlal_high_u32(vextq_u64(second, second, 1), halves.val[0], halves.val[1]);
#else
    uint64x2_t const second_sum = vmlal_u32(vextq_u64(second, second, 1), vget_high_u32(halves.val[0]), vget_high_u32(halves.val[1]));
#endif
    values.vectors[index] = vaddq_u64(values.vectors[index], first_sum);
    values.vectors[index + 1] = vaddq_u64(values.vectors[index + 1], second_sum);
  }
#if defined(__aarch64__)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    std::size_t const lane = (2 * vector_count) + index;
    auto const word = xxh3_detail::load<std::uint64_t>(stripe + (8 * lane));
    std::uint64_t const key = word ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane));
    values.scalars[index ^ 1U] += word;
    values.scalars[index] += (key & low_half_mask) * (key >> 32U);
  }
#endif
}

// The 32-bit products with the prime in the high halves give the high word of the 64-bit product; the low halves add their full product.
inline void stripe_kernel::scramble(lanes &values, std::byte const *secret) noexcept {
  uint32x2_t const prime = vdup_n_u32(primes_32::prime_1);
  uint32x4_t const prime_high = vreinterpretq_u32_u64(vdupq_n_u64(std::uint64_t{primes_32::prime_1} << 32U));
  for(std::size_t index = 0; index < vector_count; ++index) {
    uint64x2_t const mixed = veorq_u64(values.vectors[index], vshrq_n_u64(values.vectors[index], scramble_shift));
    uint64x2_t const key = veorq_u64(mixed, load_vector(secret + (16 * index)));
    uint32x4_t const high = vmulq_u32(vreinterpretq_u32_u64(key), prime_high);
    values.vectors[index] = vmlal_u32(vreinterpretq_u64_u32(high), vmovn_u64(key), prime);
  }
#if defined(__aarch64__)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    std::size_t const lane = (2 * vector_count) + index;
    std::uint64_t const accumulator = values.scalars[index] ^ (values.scalars[index] >> scramble_shift);
    values.scalars[index] = (accumulator ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane))) * primes_32::prime_1;
  }
#endif
}

} // namespace checksum::xxh3_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_XXH3_HPP
