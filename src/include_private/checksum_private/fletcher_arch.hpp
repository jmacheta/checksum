#ifndef CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP

// CPU kernels of the Fletcher checksums and Adler-32, from the directory arch.hpp selects; without the instruction-set
// extensions, or with CHECKSUM_ACCELERATION defined to 0, the portable loops sum everything. kernel<Bits> sums
// little-endian values of Bits bits: 8 for Fletcher-16 and Adler-32, 16 for Fletcher-32, 32 for Fletcher-64. Each
// architecture header specializes it with:
// - available = true; static chunk_sums sum(std::byte const *data, std::size_t blocks), for 1 to max_blocks blocks
// - block_size: bytes per block; max_blocks
// - minimum_size: shortest input for the kernel (max: never)

#include <checksum_private/arch.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::fletcher_detail {

// What a kernel summed over n values v_0 .. v_(n-1): their sum, and the sum of (n - i) * v_i, which sum2 gains on top of n * sum1.
struct chunk_sums {
  std::uint64_t sum = 0;
  std::uint64_t weighted = 0;
};

// Both sums of a checksum, each below 2^32.
struct sum_pair {
  std::uint64_t sum1 = 0;
  std::uint64_t sum2 = 0;
};

// Adds the whole kernel blocks at the start of data to sums, a chunk of at most max_blocks blocks per kernel call, each
// sum reduced by reduce() after every chunk. Returns the number of bytes taken.
template <unsigned Bits, class Reduce> std::size_t sum_chunks(sum_pair &sums, std::span<std::byte const> data, Reduce reduce) noexcept;

} // namespace checksum::fletcher_detail

#if defined(CHECKSUM_ARCH_X86_64)
#include <checksum_private/x86_64/fletcher.hpp>
#elif defined(CHECKSUM_ARCH_ARM)
#include <checksum_private/arm/fletcher.hpp>
#else
#include <checksum_private/generic/fletcher.hpp>
#endif

namespace checksum::fletcher_detail {

template <unsigned Bits, class Reduce> std::size_t sum_chunks(sum_pair &sums, std::span<std::byte const> data, Reduce reduce) noexcept {
  using chunk_kernel = kernel<Bits>;
  constexpr std::uint64_t values_per_block = chunk_kernel::block_size * 8 / Bits;
  constexpr std::uint64_t max_values = chunk_kernel::max_blocks * values_per_block;
  constexpr std::uint64_t largest_value = (std::uint64_t{1} << Bits) - 1;
  // The weighted sum of a chunk fits 64 bits, and so does sum2 plus max_values * sum1.
  static_assert(max_values * (max_values + 1) / 2 <= std::numeric_limits<std::uint64_t>::max() / largest_value);
  static_assert(max_values <= std::uint64_t{1} << 31U);
  std::byte const *position = data.data();
  std::size_t blocks = data.size() / chunk_kernel::block_size;
  std::size_t const size = blocks * chunk_kernel::block_size;
  while(blocks != 0) {
    std::size_t const count = std::min<std::size_t>(blocks, chunk_kernel::max_blocks);
    chunk_sums const chunk = chunk_kernel::sum(position, count);
    sums.sum2 = reduce(sums.sum2 + (count * values_per_block * sums.sum1) + reduce(chunk.weighted));
    sums.sum1 = reduce(sums.sum1 + chunk.sum);
    blocks -= count;
    position += count * chunk_kernel::block_size;
  }
  return size;
}

} // namespace checksum::fletcher_detail

#endif // CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
