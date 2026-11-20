#ifndef CHECKSUM_PRIVATE_RISCV64_INTERNET_HPP
#define CHECKSUM_PRIVATE_RISCV64_INTERNET_HPP

// RISC-V vector kernel of the Internet checksum (V extension, little-endian RV64), for the riscv_v preset and any vector
// length. Included only by internet_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

#if CHECKSUM_PRESET_HAS(RISCV_V)

#include <riscv_vector.h>

namespace checksum::internet_detail {

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_v))
struct block_kernel<Isa> {
  static constexpr bool available = true;
  // Not measured on hardware: one strip-mined pass costs a few instructions, so the kernel starts at 64 bytes.
  static constexpr std::size_t minimum_size = 64;
  static block_total sum(std::span<std::byte const> data) noexcept;
};

// Each 64-bit lane takes one 32-bit word per iteration, and up to 2^13 lanes (VLEN 65536) are reduced together: passes of
// 2^18 iterations keep the reduction below 2^63.
inline constexpr std::size_t rvv_pass_iterations = std::size_t{1} << 18U;

// All whole 32-bit words, widened and added into 64-bit lanes (vwaddu.wv), then reduced (vredsum). Not block_kernel::sum()
// itself: GCC takes the target attribute from the first declaration, which has none.
template <isa Isa> [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] block_total vector_sum(std::span<std::byte const> data) noexcept;

template <isa Isa> [[gnu::target(CHECKSUM_TARGET_RISCV_V)]] block_total vector_sum(std::span<std::byte const> data) noexcept {
  std::size_t const message_size = data.size();
  auto const *position = reinterpret_cast<std::uint8_t const *>(data.data());
  std::size_t words = data.size() / 4;
  data = data.subspan(words * 4);
  // e64m8 holds as many lanes as e32m4 (VLEN / 8). Clang 20 and older define the e64 vsetvl macros only when the whole
  // translation unit has 64-bit elements (-march with V), not in a target("arch=+v") function.
  std::size_t const lanes = __riscv_vsetvlmax_e32m4();
  std::uint64_t total = 0;
  while(words != 0) {
    vuint64m8_t sum = __riscv_vmv_v_x_u64m8(0, lanes);
    for(std::size_t iteration = 0; iteration < rvv_pass_iterations && words != 0; ++iteration) {
      std::size_t const length = __riscv_vsetvl_e32m4(words);
      // Byte loads have no alignment requirement; the reinterpretation reads little-endian words.
      vuint32m4_t const value = __riscv_vreinterpret_v_u8m4_u32m4(__riscv_vle8_v_u8m4(position, length * 4));
      sum = __riscv_vwaddu_wv_u64m8_tu(sum, sum, value, length);
      position += length * 4;
      words -= length;
    }
    total += fold(__riscv_vmv_x_s_u64m1_u64(__riscv_vredsum_vs_u64m8_u64m1(sum, __riscv_vmv_s_x_u64m1(0, 1), lanes)));
  }
  return {.sum = total, .size = message_size - data.size()};
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_v))
block_total block_kernel<Isa>::sum(std::span<std::byte const> data) noexcept {
  return vector_sum<Isa>(data);
}

} // namespace checksum::internet_detail

#endif

#endif // CHECKSUM_PRIVATE_RISCV64_INTERNET_HPP
