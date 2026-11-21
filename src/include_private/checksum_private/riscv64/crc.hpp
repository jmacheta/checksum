#ifndef CHECKSUM_PRIVATE_RISCV64_CRC_HPP
#define CHECKSUM_PRIVATE_RISCV64_CRC_HPP

// RISC-V kernel: folding with Zbc carry-less multiplication from isa::riscv_zbc on little-endian RV64; RISC-V has no
// CRC instruction. Included only by crc_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::crc_detail {
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header): a private header, included only by src/crc/*.cpp.
namespace {

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_zbc))
struct folding_kernel<Isa> {
  static constexpr bool available = true;
  static constexpr std::size_t minimum_size = folding_minimum_size;
  static constexpr std::size_t wide_minimum_size = std::numeric_limits<std::size_t>::max();
  template <bool Reflected>
  static std::uint64_t fold(folding_constants const &constants, std::uint64_t remainder, std::span<std::byte const> data) noexcept;
};

// Fold kernel of fold_blocks_with(): 128-bit polynomials in an unsigned __int128, low lane in the low half.
template <isa Isa> struct clmul_kernel {
  __extension__ using vector = unsigned __int128;

  static constexpr bool wide = false;

  // Byte 0 lowest if reflected, byte-reversed otherwise.
  template <bool Reflected> static vector load(std::span<std::byte const> data) noexcept;

  static vector pair(folding_pair const &constants) noexcept;

  // The register goes into the low lane if reflected, the high lane otherwise.
  template <bool Reflected> static vector first_block(std::span<std::byte const> data, std::uint64_t remainder) noexcept;

  // Low lane times the low constant xor high lane times the high constant, plus next.
  static vector fold(vector accumulator, vector constants, vector next) noexcept;

  // In message order, the accumulator's first size bytes (zero-extended in front) fold over one block onto its other
  // bytes followed by the last size message bytes.
  template <bool Reflected> static vector partial(vector accumulator, vector by_one, std::span<std::byte const> last, std::size_t size) noexcept;

  static vector multiply(std::uint64_t first, std::uint64_t second) noexcept;

  template <int Lane> static std::uint64_t lane(vector value) noexcept;

  // A * x^64 mod P: the high half folded over 128 bits, then a Barrett reduction. Reflected products carry an extra
  // factor x (see make_folding_constants()), hence the one-bit shifts.
  template <bool Reflected> static std::uint64_t reduce(vector accumulator, folding_constants const &constants) noexcept;
};

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::load(std::span<std::byte const> data) noexcept {
  constexpr std::size_t half = 8;
  std::uint64_t const first = load_little_endian<half>(data);
  std::uint64_t const second = load_little_endian<half>(data.subspan(half));
  if constexpr(Reflected) {
    return (vector{second} << 64U) | first;
  } else {
    return (vector{std::byteswap(first)} << 64U) | std::byteswap(second);
  }
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::pair(folding_pair const &constants) noexcept {
  return (vector{constants[1]} << 64U) | constants[0];
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::first_block(std::span<std::byte const> data,
                                                                                       std::uint64_t remainder) noexcept {
  return load<Reflected>(data) ^ (Reflected ? vector{remainder} : vector{remainder} << 64U);
}

template <isa Isa>
CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::fold(vector accumulator, vector constants, vector next) noexcept {
  return multiply(lane<0>(accumulator), lane<0>(constants)) ^ multiply(lane<1>(accumulator), lane<1>(constants)) ^ next;
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::partial(vector accumulator, vector by_one, std::span<std::byte const> last,
                                                                                   std::size_t size) noexcept {
  // Reflected, byte i is bits 8i..8i+7, so moving bytes towards the message end shifts left; in normal order byte i is
  // bits 120-8i..127-8i and the shift directions swap.
  unsigned const tail_bits = 8U * static_cast<unsigned>(size);
  unsigned const head_bits = 128U - tail_bits;
  vector const tail = load<Reflected>(last);
  if constexpr(Reflected) {
    vector const tail_mask = ~vector{0} << head_bits;
    return fold(accumulator << head_bits, by_one, (accumulator >> tail_bits) | (tail & tail_mask));
  } else {
    vector const tail_mask = ~(~vector{0} << tail_bits);
    return fold(accumulator >> head_bits, by_one, (accumulator << tail_bits) | (tail & tail_mask));
  }
}

template <isa Isa>
CHECKSUM_ALWAYS_INLINE inline clmul_kernel<Isa>::vector clmul_kernel<Isa>::multiply(std::uint64_t first, std::uint64_t second) noexcept {
  // The build may lack Zbc and GCC has no target pragma on RISC-V, so enable it locally in the asm.
  std::uint64_t low = 0;
  std::uint64_t high = 0;
  asm(".option push\n.option arch, +zbc\nclmul %0, %1, %2\n.option pop" : "=r"(low) : "r"(first), "r"(second));
  asm(".option push\n.option arch, +zbc\nclmulh %0, %1, %2\n.option pop" : "=r"(high) : "r"(first), "r"(second));
  return (vector{high} << 64U) | low;
}

template <isa Isa> template <int Lane> CHECKSUM_ALWAYS_INLINE inline std::uint64_t clmul_kernel<Isa>::lane(vector value) noexcept {
  return static_cast<std::uint64_t>(value >> (64U * Lane));
}

template <isa Isa>
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline std::uint64_t clmul_kernel<Isa>::reduce(vector accumulator, folding_constants const &constants) noexcept {
  constexpr unsigned top_bit = 63;
  std::uint64_t const low = lane<0>(accumulator);
  std::uint64_t const high = lane<1>(accumulator);
  if constexpr(Reflected) {
    vector const folded = multiply(low, constants.by_one[1]);
    std::uint64_t const value_high = lane<0>(folded) ^ high;
    std::uint64_t const quotient = value_high ^ (lane<0>(multiply(value_high, constants.quotient)) << 1U);
    vector const product = multiply(quotient, constants.polynomial);
    return lane<1>(folded) ^ (lane<0>(product) >> top_bit) ^ (lane<1>(product) << 1U);
  } else {
    vector const folded = multiply(high, constants.by_one[0]);
    std::uint64_t const value_high = lane<1>(folded) ^ low;
    std::uint64_t const quotient = value_high ^ lane<1>(multiply(value_high, constants.quotient));
    return lane<0>(folded) ^ lane<0>(multiply(quotient, constants.polynomial));
  }
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::riscv_zbc))
template <bool Reflected>
CHECKSUM_ALWAYS_INLINE inline std::uint64_t folding_kernel<Isa>::fold(folding_constants const &constants, std::uint64_t remainder,
                                                                      std::span<std::byte const> data) noexcept {
  return clmul_kernel<Isa>::template reduce<Reflected>(fold_blocks_with<Reflected, clmul_kernel<Isa>>(constants, remainder, data), constants);
}

} // namespace
} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_RISCV64_CRC_HPP
