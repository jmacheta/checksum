// Out-of-line 64-bit multiplication fold for targets without a native 128-bit type.

#include <checksum/xxh3.hpp>

#include <cstdint>

namespace checksum::xxh3_detail {

std::uint64_t run_time_multiply_fold(std::uint64_t left, std::uint64_t right) noexcept {
  hash128 const product = multiply_portable(left, right);
  return product.low ^ product.high;
}

} // namespace checksum::xxh3_detail
