#ifndef CHECKSUM_PRIVATE_RISCV64_FLETCHER_HPP
#define CHECKSUM_PRIVATE_RISCV64_FLETCHER_HPP

// RISC-V vector kernel of the Fletcher checksums and Adler-32 (V extension, little-endian RV64), for the riscv_v preset and
// any vector length. Included only by fletcher_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>

#if CHECKSUM_PRESET_HAS(RISCV_V)

#include <riscv_vector.h>

namespace checksum::fletcher_detail {

template <unsigned Bits, isa Isa>
  requires((Bits == 8 || Bits == 16 || Bits == 32) && isa_detail::includes(Isa, isa::riscv_v))
struct block_kernel<Bits, Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t block_size = 16;
  // 8 and 16-bit values sum in 32-bit lanes, which hold these chunks at every vector length; 32-bit values sum in 64-bit
  // lanes, and 65536 values keep the weighted sum of the chunk within 64 bits.
  static constexpr std::size_t max_blocks = Bits == 8 ? 256 : Bits == 16 ? 128 : 65'536 / 4;
  // Not measured on hardware: the thresholds of the NEON kernels.
  static constexpr std::size_t minimum_size = Bits == 8 ? 64 : Bits == 16 ? 128 : 256;
  static constexpr std::size_t alignment = 1;
  static chunk_sums sum(std::byte const *data, std::size_t blocks) noexcept;
};

// The sums of count little-endian values of Bits bits at position, in strips of one vector group each: previous gains the
// column sums before each full strip, so that the weights of a strip are the strips after it times the strip length, plus the
// lane's distance from the strip's end. A last, shorter strip gets its weights directly. Not block_kernel::sum() itself: GCC
// takes the target attribute from the first declaration, which has none.
template <unsigned Bits, isa Isa>
[[gnu::target(CHECKSUM_TARGET_RISCV_V)]] chunk_sums vector_sum(std::uint8_t const *position, std::size_t count) noexcept;

// 8 or 16-bit values zero-extended to 32-bit lanes; byte loads have no alignment requirement.
template <unsigned Bits> [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] vuint32m4_t load_widened(std::uint8_t const *position, std::size_t length) noexcept;

template <unsigned Bits>
[[gnu::target(CHECKSUM_TARGET_RISCV_V)]] vuint32m4_t load_widened(std::uint8_t const *position, std::size_t length) noexcept {
  if constexpr(Bits == 8) {
    return __riscv_vzext_vf4_u32m4(__riscv_vle8_v_u8m1(position, length), length);
  } else {
    return __riscv_vzext_vf2_u32m4(__riscv_vreinterpret_v_u8m2_u16m2(__riscv_vle8_v_u8m2(position, length * 2)), length);
  }
}

template <unsigned Bits, isa Isa>
[[gnu::target(CHECKSUM_TARGET_RISCV_V)]] chunk_sums vector_sum(std::uint8_t const *position, std::size_t count) noexcept {
  constexpr std::size_t value_size = Bits / 8;
  // Clang 20 and older define the e64 vsetvl macros only when the whole translation unit has 64-bit elements (-march with
  // V), not in a target("arch=+v") function; e32m4 has as many lanes as e64m8.
  std::size_t const lanes = __riscv_vsetvlmax_e32m4();
  std::size_t strips = count / lanes;
  std::size_t const rest = count % lanes;
  vuint64m1_t const zero = __riscv_vmv_s_x_u64m1(0, 1);
  // Lane i of weights is lanes - i, of rest_weights rest - i; lanes is at most 2^13 (VLEN 65536).
  vuint32m4_t const weights = __riscv_vrsub_vx_u32m4(__riscv_vid_v_u32m4(lanes), static_cast<std::uint32_t>(lanes), lanes);
  vuint32m4_t const rest_weights = __riscv_vrsub_vx_u32m4(__riscv_vid_v_u32m4(rest), static_cast<std::uint32_t>(rest), rest);
  std::uint64_t sum = 0;
  std::uint64_t weighted = 0;
  if constexpr(Bits == 32) {
    vuint64m8_t columns = __riscv_vmv_v_x_u64m8(0, lanes);
    vuint64m8_t previous = columns;
    for(; strips != 0; --strips, position += lanes * value_size) {
      previous = __riscv_vadd_vv_u64m8(previous, columns, lanes);
      columns = __riscv_vwaddu_wv_u64m8(columns, __riscv_vreinterpret_v_u8m4_u32m4(__riscv_vle8_v_u8m4(position, lanes * value_size)), lanes);
    }
    sum = __riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(columns, zero, lanes));
    // Modulo 2^64, like the weighted sum of the chunk, which fits.
    std::uint64_t const column_weights = __riscv_vmv_x_s_u64m1_u64(
        __riscv_vredsum_vs_u64m8_u64m1(__riscv_vmul_vv_u64m8(columns, __riscv_vzext_vf2_u64m8(weights, lanes), lanes), zero, lanes));
    weighted = (lanes * __riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(previous, zero, lanes))) + column_weights + (rest * sum);
    if(rest != 0) {
      vuint32m4_t const tail = __riscv_vreinterpret_v_u8m4_u32m4(__riscv_vle8_v_u8m4(position, rest * value_size));
      sum += __riscv_vmv_x_s_u64m1_u64(__riscv_vwredsumu_vs_u32m4_u64m1(tail, zero, rest));
      weighted += __riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(__riscv_vwmulu_vv_u64m8(tail, rest_weights, rest), zero, rest));
    }
  } else {
    // A strip has at least 16 lanes, so a chunk is at most 256 strips of bytes or 64 of 16-bit values: the lanes of previous
    // stay below 2^32.
    vuint32m4_t columns = __riscv_vmv_v_x_u32m4(0, lanes);
    vuint32m4_t previous = columns;
    for(; strips != 0; --strips, position += lanes * value_size) {
      previous = __riscv_vadd_vv_u32m4(previous, columns, lanes);
      columns = __riscv_vadd_vv_u32m4(columns, load_widened<Bits>(position, lanes), lanes);
    }
    sum = __riscv_vmv_x_s_u64m1_u64(__riscv_vwredsumu_vs_u32m4_u64m1(columns, zero, lanes));
    std::uint64_t const column_weights =
        __riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(__riscv_vwmulu_vv_u64m8(columns, weights, lanes), zero, lanes));
    weighted = (lanes * __riscv_vmv_x_s_u64m1_u64(__riscv_vwredsumu_vs_u32m4_u64m1(previous, zero, lanes))) + column_weights + (rest * sum);
    if(rest != 0) {
      vuint32m4_t const tail = load_widened<Bits>(position, rest);
      sum += __riscv_vmv_x_s_u64m1_u64(__riscv_vwredsumu_vs_u32m4_u64m1(tail, zero, rest));
      weighted += __riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(__riscv_vwmulu_vv_u64m8(tail, rest_weights, rest), zero, rest));
    }
  }
  return {.sum = sum, .weighted = weighted};
}

template <unsigned Bits, isa Isa>
  requires((Bits == 8 || Bits == 16 || Bits == 32) && isa_detail::includes(Isa, isa::riscv_v))
chunk_sums block_kernel<Bits, Isa>::sum(std::byte const *data, std::size_t blocks) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view for the vector loads.
  return vector_sum<Bits, Isa>(reinterpret_cast<std::uint8_t const *>(data), blocks * block_size / (Bits / 8));
}

} // namespace checksum::fletcher_detail

#endif

#endif // CHECKSUM_PRIVATE_RISCV64_FLETCHER_HPP
