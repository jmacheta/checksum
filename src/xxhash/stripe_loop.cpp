// Run-time stripe loop of XXH32 and XXH64, and the run-time XXH32 hash.

#include <checksum/xxhash.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::xxhash_detail {

// Not inlined: the aliasing that keeps the lanes scalar holds only behind a call (with LTO, Clang vectorized XXH32 and lost 38 %).
template <unsigned Width>
[[gnu::noinline]] word<Width> stripe_loop(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept {
  fold_stripes<Width>(lanes, data);
  return converged ? converge<Width>(lanes) : 0;
}

std::uint32_t run_time_xxh32(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % stripe_size<32>);
  lane_array<32> lanes = initial_lanes<32>(seed);
  fold_stripes<32>(lanes, data.first(whole));
  return finish<32>(converge<32>(lanes), seed, data.size(), data.subspan(whole));
}

template std::uint32_t stripe_loop<32>(lane_array<32> &, std::span<std::byte const>, bool) noexcept;
template std::uint64_t stripe_loop<64>(lane_array<64> &, std::span<std::byte const>, bool) noexcept;

} // namespace checksum::xxhash_detail
