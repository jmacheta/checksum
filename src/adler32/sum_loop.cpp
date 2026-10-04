// Run-time loop of the Adler-32 checksum: the loops of the Fletcher checksums on bytes, modulo 65521.

#include <checksum/adler32.hpp>
#include <checksum_private/fletcher_loops.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::adler32_detail {

adler32_state sum_loop(adler32_state state, std::span<std::byte const> data) noexcept {
  if(fletcher_detail::runs_kernel<std::uint8_t>(data.size())) {
    return fletcher_detail::sum_long<std::uint8_t, modulus>(state, data);
  }
  return fletcher_detail::sum_portable<std::uint8_t, modulus>(state, data);
}

} // namespace checksum::adler32_detail
