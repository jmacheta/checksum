#ifndef CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP

// CPU kernels of the Fletcher checksums and Adler-32, from the directory arch.hpp selects; without the instruction-set
// extensions, or with CHECKSUM_ACCELERATION defined to 0, the portable loops sum everything. kernel<Bits> sums
// little-endian values of Bits bits (8 for Fletcher-16 and Adler-32, 16 for Fletcher-32, 32 for Fletcher-64), and each
// architecture header specializes it with:
// - available = true; static chunk_sums sum(std::byte const *data, std::size_t blocks), for 1 to max_blocks blocks
// - block_size: bytes per block; max_blocks
// - minimum_size: shortest input for the kernel (max: never)
// - for kernel<8> also alignment: sum_chunks() adds single bytes up to an address of that alignment before the kernel

#include <checksum_private/arch.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

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

// Adds the whole kernel blocks at the start of data to sums (each below 2^32), a chunk of at most max_blocks blocks per
// kernel call, each sum reduced modulo Modulus after every chunk. Returns the number of bytes taken; inlined into its one caller.
template <unsigned Bits, std::uint64_t Modulus>
[[gnu::always_inline]] inline std::size_t sum_chunks(sum_pair &sums, std::span<std::byte const> data) noexcept;

// value modulo Modulus. The high half of a 64-bit value folds onto the low one with the weight 2^32 modulo Modulus, so that
// the constant % is a 32-bit multiplication on every target.
template <std::uint64_t Modulus, class Word> Word reduce_chunk(Word value) noexcept;

} // namespace checksum::fletcher_detail

#if defined(CHECKSUM_ARCH_X86_64)
#include <checksum_private/x86_64/fletcher.hpp>
#elif defined(CHECKSUM_ARCH_ARM)
#include <checksum_private/arm/fletcher.hpp>
#else
#include <checksum_private/generic/fletcher.hpp>
#endif

namespace checksum::fletcher_detail {

template <std::uint64_t Modulus, class Word> Word reduce_chunk(Word value) noexcept {
  if constexpr(sizeof(Word) > sizeof(std::uint32_t)) {
    constexpr std::uint64_t high_weight = (std::uint64_t{1} << 32U) % Modulus;
    while((value >> 32U) != 0) {
      value = (value & 0xFFFF'FFFFU) + ((value >> 32U) * high_weight);
    }
  }
  return static_cast<std::uint32_t>(value) % static_cast<std::uint32_t>(Modulus);
}

template <unsigned Bits, std::uint64_t Modulus>
[[gnu::always_inline]] inline std::size_t sum_chunks(sum_pair &sums, std::span<std::byte const> data) noexcept {
  using chunk_kernel = kernel<Bits>;
  constexpr std::uint64_t values_per_block = chunk_kernel::block_size * 8 / Bits;
  constexpr std::uint64_t max_values = chunk_kernel::max_blocks * values_per_block;
  constexpr std::uint64_t largest_value = (std::uint64_t{1} << Bits) - 1;
  constexpr std::uint64_t largest_weighted = max_values * (max_values + 1) / 2 * largest_value;
  constexpr std::uint64_t largest_sum = std::bit_ceil(Modulus) - 1;
  // The weighted sum of a chunk fits 64 bits, and so does sum2 plus max_values * sum1.
  static_assert(max_values * (max_values + 1) / 2 <= std::numeric_limits<std::uint64_t>::max() / largest_value);
  static_assert(max_values <= std::uint64_t{1} << 31U);
  // 32-bit arithmetic where every intermediate result fits, so that 32-bit targets reduce without 64-bit operations.
  constexpr std::uint64_t largest_word = std::numeric_limits<std::uint32_t>::max();
  using word = std::conditional_t<largest_weighted <= largest_word && (max_values + 2) * largest_sum <= largest_word &&
                                      largest_sum + (max_values * largest_value) <= largest_word,
                                  std::uint32_t, std::uint64_t>;
  auto sum1 = static_cast<word>(sums.sum1);
  auto sum2 = static_cast<word>(sums.sum2);
  std::byte const *position = data.data();
  std::size_t head = 0;
  if constexpr(Bits == 8) {
    if constexpr(chunk_kernel::alignment > 1) {
      // Bytes are single blocks, so the bytes before the alignment of the kernel can go first, one by one.
      head = std::min(data.size(), (0 - reinterpret_cast<std::uintptr_t>(position)) & (chunk_kernel::alignment - 1));
      for(std::byte const byte : data.first(head)) {
        sum1 += std::to_integer<word>(byte);
        sum2 += sum1;
      }
      sum1 = reduce_chunk<Modulus>(sum1);
      sum2 = reduce_chunk<Modulus>(sum2);
      position += head;
    }
  }
  std::size_t blocks = (data.size() - head) / chunk_kernel::block_size;
  std::size_t const size = head + (blocks * chunk_kernel::block_size);
  while(blocks != 0) {
    std::size_t const count = std::min<std::size_t>(blocks, chunk_kernel::max_blocks);
    chunk_sums const chunk = chunk_kernel::sum(position, count);
    sum2 =
        reduce_chunk<Modulus>(static_cast<word>(sum2 + (count * values_per_block * sum1) + reduce_chunk<Modulus>(static_cast<word>(chunk.weighted))));
    sum1 = reduce_chunk<Modulus>(static_cast<word>(sum1 + chunk.sum));
    blocks -= count;
    position += count * chunk_kernel::block_size;
  }
  sums = {.sum1 = sum1, .sum2 = sum2};
  return size;
}

} // namespace checksum::fletcher_detail

#endif // CHECKSUM_PRIVATE_FLETCHER_ARCH_HPP
