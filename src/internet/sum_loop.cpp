// Run-time loop of the Internet checksum, with the kernel that the build's compiler flags enable.

#include <checksum_private/internet_loops.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

// Words in native byte order: on little-endian targets this sums byte-swapped 16-bit words, and the swap of their sum is the big-endian sum.
std::uint16_t sum_loop(std::span<std::byte const> data) noexcept { return sum_any(data); }

} // namespace checksum::internet_detail
