#ifndef CHECKSUM_PRIVATE_RISCV64_XXH3_HPP
#define CHECKSUM_PRIVATE_RISCV64_XXH3_HPP

// RISC-V vector kernel of the XXH3 stripe loop (V extension, little-endian RV64), for the riscv_v preset and vector lengths from
// 128 bits. The riscv_v source defines CHECKSUM_PRESET_RISCV_V. Included only by xxh3_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/presets.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::xxh3_detail {

template <isa Isa> inline constexpr bool stripe_kernel_available = isa_detail::includes(Isa, isa::riscv_v);

// Not measured on hardware: one stripe takes about ten vector instructions instead of about fifty scalar ones, from the first stripe.
template <isa Isa>
inline constexpr std::size_t stripe_kernel_minimum_size = stripe_kernel_available<Isa> ? 0 : std::numeric_limits<std::size_t>::max();

} // namespace checksum::xxh3_detail

#if defined(__riscv) && defined(CHECKSUM_PRESET_RISCV_V)

#include <riscv_vector.h>

namespace checksum::xxh3_detail {

// The eight accumulators in one register group: four registers hold eight 64-bit lanes at the smallest vector length, 128 bits.
template <isa Isa> struct stripe_kernel {
  using lanes = vuint64m4_t;
  [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] static lanes load(accumulator_array const &accumulators) noexcept;
  [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] static void scramble(lanes &values, std::byte const *secret) noexcept;
};

inline constexpr std::size_t rvv_lanes = std::tuple_size_v<accumulator_array>;

// The 64 bytes at data as eight little-endian 64-bit lanes; byte loads have no alignment requirement.
template <isa Isa> [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] vuint64m4_t load_stripe(std::byte const *data) noexcept;

template <isa Isa> [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] vuint64m4_t load_stripe(std::byte const *data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  return __riscv_vreinterpret_v_u8m4_u64m4(__riscv_vle8_v_u8m4(reinterpret_cast<std::uint8_t const *>(data), stripe_size));
}

template <isa Isa> stripe_kernel<Isa>::lanes stripe_kernel<Isa>::load(accumulator_array const &accumulators) noexcept {
  return __riscv_vle64_v_u64m4(accumulators.data(), rvv_lanes);
}

template <isa Isa> void stripe_kernel<Isa>::store(accumulator_array &accumulators, lanes const &values) noexcept {
  __riscv_vse64_v_u64m4(accumulators.data(), values, rvv_lanes);
}

// Each lane adds the product of the 32-bit halves of data ^ secret (vmacc), and the data of its neighbor (vrgather).
template <isa Isa> void stripe_kernel<Isa>::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  vuint64m4_t const data = load_stripe<Isa>(stripe);
  vuint64m4_t const key = __riscv_vxor_vv_u64m4(data, load_stripe<Isa>(secret), rvv_lanes);
  vuint64m4_t const neighbors = __riscv_vxor_vx_u64m4(__riscv_vid_v_u64m4(rvv_lanes), 1, rvv_lanes);
  values = __riscv_vmacc_vv_u64m4(values, __riscv_vand_vx_u64m4(key, low_half_mask, rvv_lanes), __riscv_vsrl_vx_u64m4(key, 32, rvv_lanes), rvv_lanes);
  values = __riscv_vadd_vv_u64m4(values, __riscv_vrgather_vv_u64m4(data, neighbors, rvv_lanes), rvv_lanes);
}

template <isa Isa> void stripe_kernel<Isa>::scramble(lanes &values, std::byte const *secret) noexcept {
  vuint64m4_t const mixed = __riscv_vxor_vv_u64m4(values, __riscv_vsrl_vx_u64m4(values, scramble_shift, rvv_lanes), rvv_lanes);
  values = __riscv_vmul_vx_u64m4(__riscv_vxor_vv_u64m4(mixed, load_stripe<Isa>(secret), rvv_lanes), primes_32::prime_1, rvv_lanes);
}

} // namespace checksum::xxh3_detail

#endif

#endif // CHECKSUM_PRIVATE_RISCV64_XXH3_HPP
