#ifndef CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
#define CHECKSUM_PRIVATE_INTERNET_ARCH_HPP

// CPU kernels of the Internet checksum, from the directory of the target architecture. Each architecture header defines,
// for a preset Isa:
// - block_sum_available<Isa>; if true, block_sum<Isa>()
// - block_sum_minimum_size<Isa>: shortest input for block_sum<Isa>() (max: never)

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

// What a kernel summed: the leading size bytes of the message (an even number, or all of it) as native-order words.
struct block_total {
  std::uint64_t sum = 0; // unfolded
  std::size_t size = 0;
};

// Sums leading whole words of data.
template <isa Isa> block_total block_sum(std::span<std::byte const> data) noexcept;

} // namespace checksum::internet_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/internet.hpp>
#elif defined(__aarch64__) || defined(__arm__)
#include <checksum_private/arm/internet.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/internet.hpp>
#else
#include <checksum_private/generic/internet.hpp>
#endif

#endif // CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
