#ifndef CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP

// CPU kernels of the Fletcher checksums and Adler-32, from the directory of the target architecture. kernel<Bits, Isa>
// sums little-endian values of Bits bits (8 for Fletcher-16 and Adler-32, 16 for Fletcher-32, 32 for Fletcher-64). The
// primary template means no kernel; an architecture header specializes it for the presets with one, with:
// - available = true; static chunk_sums sum(std::byte const *data, std::size_t blocks), for 1 to max_blocks blocks
// - minimum_size: shortest input for the kernel
// - block_size: bytes per block; max_blocks
// - for kernel<8, Isa> also alignment: sum_chunks() of fletcher_loops.hpp adds single bytes up to an address of that
//   alignment before the kernel
// group_kernel<Bits, Isa>, with the same interface, runs instead of kernel from its own minimum_size (sum_long() of
// fletcher_loops.hpp).

#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::fletcher_detail {

// What a kernel summed over n values v_0 .. v_(n-1): their sum, and the sum of (n - i) * v_i, which sum2 gains on top of n * sum1.
struct chunk_sums {
  std::uint64_t sum = 0;
  std::uint64_t weighted = 0;
};

template <unsigned Bits, isa Isa> struct kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

template <unsigned Bits, isa Isa> struct group_kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

} // namespace checksum::fletcher_detail

// std::conditional_t of __m128i or __m256i drops their __may_alias__ attribute, which kernel vectors do not need.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/fletcher.hpp>
#elif defined(CHECKSUM_ARM64) || defined(__arm__)
#include <checksum_private/arm/fletcher.hpp>
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
