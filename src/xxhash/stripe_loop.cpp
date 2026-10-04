// Run-time stripe loop of XXH32 and XXH64, and the run-time hashes.

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

template <unsigned Width> word<Width> run_time_hash(std::span<std::byte const> data, word<Width> seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % stripe_size<Width>);
  lane_array<Width> lanes = initial_lanes<Width>(seed);
  fold_stripes<Width>(lanes, data.first(whole));
  if constexpr(Width == 64) {
    // Hides how the lanes were computed: GCC otherwise merges the last multiplication of each stripe with the first one of converge(),
    // which keeps two values per lane in the stripe loop and made XXH64 7 % slower at 1 to 2 KiB on x86-64.
    asm("" : "+r"(lanes[0]));
    asm("" : "+r"(lanes[1]));
    asm("" : "+r"(lanes[2]));
    asm("" : "+r"(lanes[3]));
  }
  return finish<Width>(converge<Width>(lanes), seed, data.size(), data.subspan(whole));
}

template std::uint32_t stripe_loop<32>(lane_array<32> &, std::span<std::byte const>, bool) noexcept;
template std::uint64_t stripe_loop<64>(lane_array<64> &, std::span<std::byte const>, bool) noexcept;
template std::uint32_t run_time_hash<32>(std::span<std::byte const>, std::uint32_t) noexcept;
template std::uint64_t run_time_hash<64>(std::span<std::byte const>, std::uint64_t) noexcept;

} // namespace checksum::xxhash_detail
