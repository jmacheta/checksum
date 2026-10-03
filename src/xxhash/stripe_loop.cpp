// Run-time stripe loop of XXH32 and XXH64.

#include <checksum/xxhash.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::xxhash_detail {

// Not inlined: the aliasing that keeps the lanes scalar holds only behind a call (with LTO, Clang vectorized XXH32 and lost 38 %).
template <unsigned Width> [[gnu::noinline]] word<Width> stripe_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  fold_stripes<Width>(lanes, data);
  return converge<Width>(lanes);
}

template std::uint32_t stripe_loop<32>(lane_array<32> &, std::span<std::byte const>) noexcept;
template std::uint64_t stripe_loop<64>(lane_array<64> &, std::span<std::byte const>) noexcept;

} // namespace checksum::xxhash_detail
