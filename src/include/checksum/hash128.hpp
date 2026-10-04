#ifndef CHECKSUM_HASH128_HPP
#define CHECKSUM_HASH128_HPP

#include <cstdint>

/**
 * @addtogroup checksum
 * @{
 */

namespace checksum {

/// A 128-bit hash value, the result of XXH3-128 and MurmurHash3_x64_128.
struct hash128 {
  std::uint64_t low = 0;  ///< The lower 64 bits.
  std::uint64_t high = 0; ///< The upper 64 bits.

  /// Equal if both halves are.
  [[nodiscard]] constexpr bool operator==(hash128 const &) const noexcept = default;
};

} // namespace checksum

///@}

#endif // CHECKSUM_HASH128_HPP
