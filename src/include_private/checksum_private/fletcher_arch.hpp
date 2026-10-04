#ifndef CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP

// CPU kernels of the Fletcher checksums and Adler-32, from the directory arch.hpp selects; without the instruction-set
// extensions, or with CHECKSUM_ACCELERATION defined to 0, the portable loops sum everything. kernel<Bits> sums
// little-endian values of Bits bits (8 for Fletcher-16 and Adler-32, 16 for Fletcher-32, 32 for Fletcher-64), and each
// architecture header specializes it with:
// - available = true; static chunk_sums sum(std::byte const *data, std::size_t blocks), for 1 to max_blocks blocks
// - block_size: bytes per block; max_blocks
// - minimum_size: shortest input for the kernel (max: never)
// - for kernel<8> also alignment: sum_chunks() of fletcher_loops.hpp adds single bytes up to an address of that alignment
//   before the kernel

#include <checksum_private/arch.hpp>

#include <cstdint>

namespace checksum::fletcher_detail {

// What a kernel summed over n values v_0 .. v_(n-1): their sum, and the sum of (n - i) * v_i, which sum2 gains on top of n * sum1.
struct chunk_sums {
  std::uint64_t sum = 0;
  std::uint64_t weighted = 0;
};

} // namespace checksum::fletcher_detail

#if defined(CHECKSUM_ARCH_X86_64)
#include <checksum_private/x86_64/fletcher.hpp>
#elif defined(CHECKSUM_ARCH_ARM)
#include <checksum_private/arm/fletcher.hpp>
#else
#include <checksum_private/generic/fletcher.hpp>
#endif

#endif // CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
