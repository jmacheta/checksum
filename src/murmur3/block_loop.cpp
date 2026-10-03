// Run-time block loop of MurmurHash3_x86_32 and MurmurHash3_x64_128.

#include <checksum/murmur3.hpp>

#include <cstddef>
#include <span>

namespace checksum::murmur3_detail {

template <unsigned Width> lane_array<Width> block_loop(lane_array<Width> lanes, std::span<std::byte const> data) noexcept {
  return fold_blocks<Width>(lanes, data);
}

template lane_array<32> block_loop<32>(lane_array<32>, std::span<std::byte const>) noexcept;
template lane_array<128> block_loop<128>(lane_array<128>, std::span<std::byte const>) noexcept;

} // namespace checksum::murmur3_detail
