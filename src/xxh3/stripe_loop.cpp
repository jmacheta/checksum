// Run-time stripe loop and 64-bit multiplication of XXH3.

#include <checksum/xxh3.hpp>
#include <checksum_private/xxh3_arch.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::xxh3_detail {

void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                 std::byte const *last_stripe) noexcept {
  if constexpr(stripe_kernel_available) {
    if(data.size() >= stripe_kernel_minimum_size) {
      fold_stripes<stripe_kernel>(accumulators, data, block_stripe, secret, last_stripe);
      return;
    }
  }
  fold_stripes<portable_kernel>(accumulators, data, block_stripe, secret, last_stripe);
}

std::uint64_t run_time_multiply_fold(std::uint64_t left, std::uint64_t right) noexcept {
  xxh3_hash128 const product = multiply_portable(left, right);
  return product.low ^ product.high;
}

} // namespace checksum::xxh3_detail
