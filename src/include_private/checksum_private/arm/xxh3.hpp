#ifndef CHECKSUM_PRIVATE_ARM_XXH3_HPP
#define CHECKSUM_PRIVATE_ARM_XXH3_HPP

// NEON kernel of the XXH3 stripe loop on little-endian AArch64 and on the thumb2_neon/thumb2_crc presets. Included only by
// xxh3_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>
#include <checksum_private/vector_array.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

#if defined(CHECKSUM_ARM64) || CHECKSUM_PRESET_HAS(NEON)

#include <arm_neon.h>

namespace checksum::xxh3_detail {

template <isa Isa>
inline constexpr bool neon_kernel =
    std::endian::native == std::endian::little && (target_detail::arm64_target || isa_detail::includes(Isa, isa::thumb2_neon));

#if defined(CHECKSUM_ARM64)
// On a Cortex-A72 four lanes in NEON and four scalar ones beat six and two, all eight in NEON, and the portable loop.
inline constexpr std::size_t vector_count = 2;
#else
inline constexpr std::size_t vector_count = 4;
#endif

template <isa Isa>
  requires(neon_kernel<Isa>)
struct stripe_kernel<Isa> {
  static constexpr bool available = true;
  // With GCC on a Cortex-A72 the kernel beats or ties the portable loop from 256 bytes, the shortest input that reaches it.
  static constexpr std::size_t minimum_size = 0;
  struct lanes {
    isa_detail::vector_array<uint64x2_t, vector_count> vectors;
#if defined(CHECKSUM_ARM64)
    std::array<std::uint64_t, 8 - (2 * vector_count)> scalars;
#endif
  };
  static lanes load(accumulator_array const &accumulators) noexcept;
  static void store(accumulator_array &accumulators, lanes const &values) noexcept;
  static void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  static void scramble(lanes &values, std::byte const *secret) noexcept;
};

// The 16 bytes at data as two little-endian 64-bit lanes.
template <isa Isa> uint64x2_t load_vector(std::byte const *data) noexcept;

// Mixes the vectors Index and Index + 1 of one stripe into their lanes: one unzip gives the low and the high 32-bit halves of
// data ^ secret for four lanes.
template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void accumulate_vector_pair(typename stripe_kernel<Isa>::lanes &values, std::byte const *stripe,
                                                          std::byte const *secret) noexcept;

// Scrambles vector Index. The 32-bit products with the prime in the high halves give the high word of the 64-bit product; the low
// halves add their full product.
template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void scramble_vector(typename stripe_kernel<Isa>::lanes &values, std::byte const *secret) noexcept;

#if defined(CHECKSUM_ARM64)
// Mixes scalar lane Index of one stripe into the scalars, as accumulate_lane() does.
template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void accumulate_scalar(typename stripe_kernel<Isa>::lanes &values, std::byte const *stripe,
                                                     std::byte const *secret) noexcept;

// Scrambles scalar lane Index, as portable_kernel::scramble() does.
template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void scramble_scalar(typename stripe_kernel<Isa>::lanes &values, std::byte const *secret) noexcept;
#endif

template <isa Isa> uint64x2_t load_vector(std::byte const *data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  return vreinterpretq_u64_u8(vld1q_u8(reinterpret_cast<std::uint8_t const *>(data)));
}

template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void accumulate_vector_pair(typename stripe_kernel<Isa>::lanes &values, std::byte const *stripe,
                                                          std::byte const *secret) noexcept {
  uint64x2_t const first = load_vector<Isa>(stripe + (16 * Index));
  uint64x2_t const second = load_vector<Isa>(stripe + (16 * Index) + 16);
  uint64x2_t const first_key = veorq_u64(first, load_vector<Isa>(secret + (16 * Index)));
  uint64x2_t const second_key = veorq_u64(second, load_vector<Isa>(secret + (16 * Index) + 16));
  uint32x4x2_t const halves = vuzpq_u32(vreinterpretq_u32_u64(first_key), vreinterpretq_u32_u64(second_key));
  uint64x2_t const first_sum = vmlal_u32(vextq_u64(first, first, 1), vget_low_u32(halves.val[0]), vget_low_u32(halves.val[1]));
#if defined(CHECKSUM_ARM64)
  uint64x2_t const second_sum = vmlal_high_u32(vextq_u64(second, second, 1), halves.val[0], halves.val[1]);
#else
  uint64x2_t const second_sum = vmlal_u32(vextq_u64(second, second, 1), vget_high_u32(halves.val[0]), vget_high_u32(halves.val[1]));
#endif
  values.vectors[Index] = vaddq_u64(values.vectors[Index], first_sum);
  values.vectors[Index + 1] = vaddq_u64(values.vectors[Index + 1], second_sum);
}

template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void scramble_vector(typename stripe_kernel<Isa>::lanes &values, std::byte const *secret) noexcept {
  uint32x2_t const prime = vdup_n_u32(primes_32::prime_1);
  uint32x4_t const prime_high = vreinterpretq_u32_u64(vdupq_n_u64(std::uint64_t{primes_32::prime_1} << 32U));
  uint64x2_t &vector = values.vectors[Index];
  uint64x2_t const key = veorq_u64(veorq_u64(vector, vshrq_n_u64(vector, scramble_shift)), load_vector<Isa>(secret + (16 * Index)));
  uint32x4_t const high = vmulq_u32(vreinterpretq_u32_u64(key), prime_high);
  vector = vmlal_u32(vreinterpretq_u64_u32(high), vmovn_u64(key), prime);
}

#if defined(CHECKSUM_ARM64)
template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void accumulate_scalar(typename stripe_kernel<Isa>::lanes &values, std::byte const *stripe,
                                                     std::byte const *secret) noexcept {
  constexpr std::size_t lane = (2 * vector_count) + Index;
  auto const word = xxh3_detail::load<std::uint64_t>(stripe + (8 * lane));
  std::uint64_t const key = word ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane));
  std::get<Index ^ 1U>(values.scalars) += word;
  std::get<Index>(values.scalars) += (key & low_half_mask) * (key >> 32U);
}

template <isa Isa, std::size_t Index>
CHECKSUM_ALWAYS_INLINE inline void scramble_scalar(typename stripe_kernel<Isa>::lanes &values, std::byte const *secret) noexcept {
  constexpr std::size_t lane = (2 * vector_count) + Index;
  std::uint64_t &scalar = std::get<Index>(values.scalars);
  scalar = (scalar ^ (scalar >> scramble_shift) ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane))) * primes_32::prime_1;
}
#endif

template <isa Isa>
  requires(neon_kernel<Isa>)
auto stripe_kernel<Isa>::load(accumulator_array const &accumulators) noexcept -> lanes {
  lanes values{};
  for(std::size_t index = 0; index < vector_count; ++index) {
    values.vectors[index] = vld1q_u64(accumulators.data() + (2 * index));
  }
#if defined(CHECKSUM_ARM64)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    values.scalars[index] = accumulators[(2 * vector_count) + index];
  }
#endif
  return values;
}

template <isa Isa>
  requires(neon_kernel<Isa>)
void stripe_kernel<Isa>::store(accumulator_array &accumulators, lanes const &values) noexcept {
  for(std::size_t index = 0; index < vector_count; ++index) {
    vst1q_u64(accumulators.data() + (2 * index), values.vectors[index]);
  }
#if defined(CHECKSUM_ARM64)
  for(std::size_t index = 0; index < values.scalars.size(); ++index) {
    accumulators[(2 * vector_count) + index] = values.scalars[index];
  }
#endif
}

// Spelled out per vector and lane: at -O2, GCC keeps the lanes of a loop that it does not unroll in memory.
template <isa Isa>
  requires(neon_kernel<Isa>)
void stripe_kernel<Isa>::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  [&]<std::size_t... Pair> CHECKSUM_ALWAYS_INLINE(std::index_sequence<Pair...>) {
    (accumulate_vector_pair<Isa, 2 * Pair>(values, stripe, secret), ...);
  }(std::make_index_sequence<vector_count / 2>{});
#if defined(CHECKSUM_ARM64)
  [&]<std::size_t... Index> CHECKSUM_ALWAYS_INLINE(std::index_sequence<Index...>) {
    (accumulate_scalar<Isa, Index>(values, stripe, secret), ...);
  }(std::make_index_sequence<std::tuple_size_v<decltype(values.scalars)>>{});
#endif
}

template <isa Isa>
  requires(neon_kernel<Isa>)
void stripe_kernel<Isa>::scramble(lanes &values, std::byte const *secret) noexcept {
  [&]<std::size_t... Index> CHECKSUM_ALWAYS_INLINE(std::index_sequence<Index...>) {
    (scramble_vector<Isa, Index>(values, secret), ...);
  }(std::make_index_sequence<vector_count>{});
#if defined(CHECKSUM_ARM64)
  [&]<std::size_t... Index> CHECKSUM_ALWAYS_INLINE(std::index_sequence<Index...>) {
    (scramble_scalar<Isa, Index>(values, secret), ...);
  }(std::make_index_sequence<std::tuple_size_v<decltype(values.scalars)>>{});
#endif
}

} // namespace checksum::xxh3_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_XXH3_HPP
