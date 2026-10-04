// Run-time block loop of MurmurHash3_x86_32 and MurmurHash3_x64_128, and the run-time x86_32 hash.

#include <checksum/murmur3.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::murmur3_detail {

// Inlining is harmless: each lane depends on the previous one, so there is nothing to vectorize.
template <unsigned Width> void block_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  lanes = fold_blocks<Width>(lanes, data);
}

std::uint32_t run_time_hash_32(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % block_size<32>);
  return finish<32>(fold_blocks<32>(initial_lanes<32>(seed), data.first(whole)), data.size(), data.subspan(whole));
}

template void block_loop<32>(lane_array<32> &, std::span<std::byte const>) noexcept;
template void block_loop<128>(lane_array<128> &, std::span<std::byte const>) noexcept;

} // namespace checksum::murmur3_detail
