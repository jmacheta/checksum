#ifndef CHECKSUM_PRIVATE_RISCV64_FLETCHER4_HPP
#define CHECKSUM_PRIVATE_RISCV64_FLETCHER4_HPP

// RISC-V vector kernel of the fletcher4 lanes (V extension, little-endian RV64), for the riscv_v preset: the four lanes of
// each sum in one register group, which holds four 64-bit lanes at every vector length. Included only by fletcher4_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>

#if CHECKSUM_PRESET_HAS(RISCV_V)

#include <riscv_vector.h>

namespace checksum::fletcher4_detail {

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_v))
struct lane_kernel<Isa> {
  static constexpr bool available = true;
  // Not measured on hardware: the threshold of the NEON kernel.
  static constexpr std::size_t minimum_size = 192;
  static lane_sums sum(std::byte const *data, std::size_t groups) noexcept;
};

// The lane sums into sums[0 .. 15], as lane_sums lays them out. Not lane_kernel::sum() itself: GCC takes the target attribute
// from the first declaration, which has none, and does not inline the always_inline std::array::data() into a vector function.
template <isa Isa>
[[gnu::target(CHECKSUM_TARGET_RISCV_V)]] void vector_lanes(std::uint64_t *sums, std::uint8_t const *data, std::size_t groups) noexcept;

template <isa Isa>
[[gnu::target(CHECKSUM_TARGET_RISCV_V)]] void vector_lanes(std::uint64_t *sums, std::uint8_t const *data, std::size_t groups) noexcept {
  constexpr std::size_t lanes = lane_count;
  vuint64m2_t sum1 = __riscv_vmv_v_x_u64m2(0, lanes);
  vuint64m2_t sum2 = sum1;
  vuint64m2_t sum3 = sum1;
  vuint64m2_t sum4 = sum1;
  for(; groups != 0; --groups, data += lanes * word_size) {
    // Byte loads have no alignment requirement; the reinterpretation reads little-endian words.
    vuint32m1_t const words = __riscv_vreinterpret_v_u8m1_u32m1(__riscv_vle8_v_u8m1(data, lanes * word_size));
    sum1 = __riscv_vwaddu_wv_u64m2(sum1, words, lanes);
    sum2 = __riscv_vadd_vv_u64m2(sum2, sum1, lanes);
    sum3 = __riscv_vadd_vv_u64m2(sum3, sum2, lanes);
    sum4 = __riscv_vadd_vv_u64m2(sum4, sum3, lanes);
  }
  __riscv_vse64_v_u64m2(sums, sum1, lanes);
  __riscv_vse64_v_u64m2(sums + lanes, sum2, lanes);
  __riscv_vse64_v_u64m2(sums + (2 * lanes), sum3, lanes);
  __riscv_vse64_v_u64m2(sums + (3 * lanes), sum4, lanes);
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_v))
lane_sums lane_kernel<Isa>::sum(std::byte const *data, std::size_t groups) noexcept {
  lane_sums lanes{};
  static_assert(sizeof(lanes) == 4 * lane_count * sizeof(std::uint64_t));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the contiguous sums of lane_sums, and a byte view for the vector loads.
  vector_lanes<Isa>(lanes[0].data(), reinterpret_cast<std::uint8_t const *>(data), groups);
  return lanes;
}

} // namespace checksum::fletcher4_detail

#endif

#endif // CHECKSUM_PRIVATE_RISCV64_FLETCHER4_HPP
