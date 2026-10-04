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
#include <tuple>
#include <utility>

namespace checksum::xxh3_detail {

inline constexpr bool stripe_kernel_available = true;

// With GCC on a Cortex-A72 the kernel beats or ties the portable loop from 256 bytes, the shortest input that reaches it.
inline constexpr std::size_t stripe_kernel_minimum_size = 0;

#if defined(__aarch64__)
// On a Cortex-A72 four lanes in NEON and four scalar ones beat six and two, all eight in NEON, and the portable loop.
inline constexpr std::size_t vector_count = 2;
#else
inline constexpr std::size_t vector_count = 4;
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

// Mixes the vectors Index and Index + 1 of one stripe into their lanes: one unzip gives the low and the high 32-bit halves of
// data ^ secret for four lanes.
template <std::size_t Index> void accumulate_vector_pair(stripe_kernel::lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;

// Scrambles vector Index. The 32-bit products with the prime in the high halves give the high word of the 64-bit product; the low
// halves add their full product.
template <std::size_t Index> void scramble_vector(stripe_kernel::lanes &values, std::byte const *secret) noexcept;

#if defined(__aarch64__)
// Mixes scalar lane Index of one stripe into the scalars, as accumulate_lane() does.
template <std::size_t Index> void accumulate_scalar(stripe_kernel::lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;

// Scrambles scalar lane Index, as portable_kernel::scramble() does.
template <std::size_t Index> void scramble_scalar(stripe_kernel::lanes &values, std::byte const *secret) noexcept;
#endif

inline uint64x2_t load_vector(std::byte const *data) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector load.
  return vreinterpretq_u64_u8(vld1q_u8(reinterpret_cast<std::uint8_t const *>(data)));
}

template <std::size_t Index>
[[gnu::always_inline]] inline void accumulate_vector_pair(stripe_kernel::lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  uint64x2_t const first = load_vector(stripe + (16 * Index));
  uint64x2_t const second = load_vector(stripe + (16 * Index) + 16);
  uint64x2_t const first_key = veorq_u64(first, load_vector(secret + (16 * Index)));
  uint64x2_t const second_key = veorq_u64(second, load_vector(secret + (16 * Index) + 16));
  uint32x4x2_t const halves = vuzpq_u32(vreinterpretq_u32_u64(first_key), vreinterpretq_u32_u64(second_key));
  uint64x2_t const first_sum = vmlal_u32(vextq_u64(first, first, 1), vget_low_u32(halves.val[0]), vget_low_u32(halves.val[1]));
#if defined(__aarch64__)
  uint64x2_t const second_sum = vmlal_high_u32(vextq_u64(second, second, 1), halves.val[0], halves.val[1]);
#else
  uint64x2_t const second_sum = vmlal_u32(vextq_u64(second, second, 1), vget_high_u32(halves.val[0]), vget_high_u32(halves.val[1]));
#endif
  std::get<Index>(values.vectors) = vaddq_u64(std::get<Index>(values.vectors), first_sum);
  std::get<Index + 1>(values.vectors) = vaddq_u64(std::get<Index + 1>(values.vectors), second_sum);
}

template <std::size_t Index> [[gnu::always_inline]] inline void scramble_vector(stripe_kernel::lanes &values, std::byte const *secret) noexcept {
  uint32x2_t const prime = vdup_n_u32(primes_32::prime_1);
  uint32x4_t const prime_high = vreinterpretq_u32_u64(vdupq_n_u64(std::uint64_t{primes_32::prime_1} << 32U));
  uint64x2_t &vector = std::get<Index>(values.vectors);
  uint64x2_t const key = veorq_u64(veorq_u64(vector, vshrq_n_u64(vector, scramble_shift)), load_vector(secret + (16 * Index)));
  uint32x4_t const high = vmulq_u32(vreinterpretq_u32_u64(key), prime_high);
  vector = vmlal_u32(vreinterpretq_u64_u32(high), vmovn_u64(key), prime);
}

#if defined(__aarch64__)
template <std::size_t Index>
[[gnu::always_inline]] inline void accumulate_scalar(stripe_kernel::lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  constexpr std::size_t lane = (2 * vector_count) + Index;
  auto const word = xxh3_detail::load<std::uint64_t>(stripe + (8 * lane));
  std::uint64_t const key = word ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane));
  std::get<Index ^ 1U>(values.scalars) += word;
  std::get<Index>(values.scalars) += (key & low_half_mask) * (key >> 32U);
}

template <std::size_t Index> [[gnu::always_inline]] inline void scramble_scalar(stripe_kernel::lanes &values, std::byte const *secret) noexcept {
  constexpr std::size_t lane = (2 * vector_count) + Index;
  std::uint64_t &scalar = std::get<Index>(values.scalars);
  scalar = (scalar ^ (scalar >> scramble_shift) ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane))) * primes_32::prime_1;
}
#endif

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

// Spelled out per vector and lane: at -O2, GCC keeps the lanes of a loop that it does not unroll in memory.
inline void stripe_kernel::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  [&]<std::size_t... Pair> [[gnu::always_inline]] (std::index_sequence<Pair...>) {
    (accumulate_vector_pair<2 * Pair>(values, stripe, secret), ...);
  }(std::make_index_sequence<vector_count / 2>{});
#if defined(__aarch64__)
  [&]<std::size_t... Index> [[gnu::always_inline]] (std::index_sequence<Index...>) {
    (accumulate_scalar<Index>(values, stripe, secret), ...);
  }(std::make_index_sequence<std::tuple_size_v<decltype(values.scalars)>>{});
#endif
}

inline void stripe_kernel::scramble(lanes &values, std::byte const *secret) noexcept {
  [&]<std::size_t... Index> [[gnu::always_inline]] (std::index_sequence<Index...>) {
    (scramble_vector<Index>(values, secret), ...);
  }(std::make_index_sequence<vector_count>{});
#if defined(__aarch64__)
  [&]<std::size_t... Index> [[gnu::always_inline]] (std::index_sequence<Index...>) {
    (scramble_scalar<Index>(values, secret), ...);
  }(std::make_index_sequence<std::tuple_size_v<decltype(values.scalars)>>{});
#endif
}

} // namespace checksum::xxh3_detail

#endif

#endif // CHECKSUM_PRIVATE_ARM_XXH3_HPP
