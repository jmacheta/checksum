// Run-time loop of the Adler-32 checksum: the loops of the Fletcher checksums on bytes, modulo 65521.

#include <checksum/adler32.hpp>
#include <checksum_private/fletcher_loops.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::adler32_detail {

adler32_state sum_loop(adler32_state state, std::span<std::byte const> data) noexcept {
  fletcher_detail::sum_pair const sums =
      fletcher_detail::sum_values<std::uint8_t, modulus>({.sum1 = reduce_once(state.sum1), .sum2 = reduce_once(state.sum2)}, data);
  return {.sum1 = static_cast<std::uint16_t>(sums.sum1), .sum2 = static_cast<std::uint16_t>(sums.sum2)};
}

} // namespace checksum::adler32_detail
