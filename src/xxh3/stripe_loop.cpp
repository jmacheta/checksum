// Run-time stripe loop of XXH3.

#include <checksum/xxh3.hpp>

#include <cstddef>
#include <span>

namespace checksum::xxh3_detail {

void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index, secret_array const &secret) noexcept {
  fold_stripes(accumulators, data, stripe_index, secret);
}

} // namespace checksum::xxh3_detail
