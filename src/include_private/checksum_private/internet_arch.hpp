#ifndef CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
#define CHECKSUM_PRIVATE_INTERNET_ARCH_HPP

// CPU kernels of the Internet checksum, from the directory of the target architecture. The primary template of
// block_kernel<Isa> means no kernel; an architecture header specializes it for the presets with one, with:
// - available = true; static block_total sum(std::span<std::byte const> data): sums leading whole words of data
// - minimum_size: shortest input for the kernel
// carry_chain<Isa> likewise, for the portable loop: available = true and static word add(word left, word right, word &carry),
// left + right + carry (0 or 1) as one add with carry, the carry out in carry.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::internet_detail {

// The widest integer the target adds natively. Its carries are worth 2^bits, which is 1 modulo 0xFFFF for any multiple of 16 bits.
using word = std::size_t;

// What a kernel summed: the leading size bytes of the message (an even number, or all of it) as native-order words.
struct block_total {
  std::uint64_t sum = 0; // unfolded
  std::size_t size = 0;
};

template <isa Isa> struct block_kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

template <isa Isa> struct carry_chain {
  static constexpr bool available = false;
};

} // namespace checksum::internet_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/internet.hpp>
#elif defined(CHECKSUM_ARM64) || defined(__arm__)
#include <checksum_private/arm/internet.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/internet.hpp>
#endif

#endif // CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
