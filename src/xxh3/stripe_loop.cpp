// Run-time stripe loop of XXH3.

#include <checksum/xxh3.hpp>
#include <checksum_private/xxh3_arch.hpp>

#include <cstddef>
#include <span>

namespace checksum::xxh3_detail {

void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index, secret_array const &secret,
                 std::byte const *last_stripe) noexcept {
  if constexpr(stripe_kernel_available) {
    if(data.size() >= stripe_kernel_minimum_size) {
      fold_stripes<stripe_kernel>(accumulators, data, stripe_index, secret, last_stripe);
      return;
    }
  }
  fold_stripes<portable_kernel>(accumulators, data, stripe_index, secret, last_stripe);
}

} // namespace checksum::xxh3_detail
