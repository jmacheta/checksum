#ifndef CHECKSUM_PRIVATE_XXHASH_LOOPS_HPP
#define CHECKSUM_PRIVATE_XXHASH_LOOPS_HPP

// Run-time stripe loop of XXH32 and XXH64 for one preset: the portable loops of xxhash.hpp and the run-time hashes. A source of
// src/xxhash/ defines CHECKSUM_PRESET (see target_begin.hpp) and includes this header once.

// Every std header of this file comes before the target region, so that their inline functions compile for the build's own target.
#include <checksum/isa.hpp>
#include <checksum/xxhash.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

#include <checksum_private/target_begin.hpp>

namespace checksum::xxhash_detail {

// Not inlined: the aliasing that keeps the lanes scalar holds only behind a call (with LTO, Clang vectorized XXH32 and lost 38 %).
template <isa Isa, unsigned Width>
CHECKSUM_NOINLINE word<Width> stripe_loop_any(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept;

// The run-time hash for preset Isa; run_time_hash() forwards to it.
template <isa Isa, unsigned Width> word<Width> run_time_hash_any(std::span<std::byte const> data, word<Width> seed) noexcept;

template <isa Isa, unsigned Width> word<Width> stripe_loop_any(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept {
  fold_stripes<Width>(lanes, data);
  return converged ? converge<Width>(lanes) : 0;
}

template <isa Isa, unsigned Width> word<Width> run_time_hash_any(std::span<std::byte const> data, word<Width> seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % stripe_size<Width>);
  lane_array<Width> lanes = initial_lanes<Width>(seed);
  fold_stripes<Width>(lanes, data.first(whole));
  if constexpr(Width == 64) {
    // Hides how the lanes were computed: GCC otherwise merges the last multiplication of each stripe with the first one of converge(),
    // which keeps two values per lane in the stripe loop and made XXH64 7 % slower at 1 to 2 KiB on x86-64.
#if defined(__GNUC__) || defined(__clang__)
    asm("" : "+r"(lanes[0]));
    asm("" : "+r"(lanes[1]));
    asm("" : "+r"(lanes[2]));
    asm("" : "+r"(lanes[3]));
#endif
  }
  return finish<Width>(converge<Width>(lanes), seed, data.size(), data.subspan(whole));
}

} // namespace checksum::xxhash_detail

#include <checksum_private/target_end.hpp>

namespace checksum::xxhash_detail {

// GCC fixes a function's target at its first declaration, which for stripe_loop() is in xxhash.hpp: it only forwards to stripe_loop_any(),
// first declared inside the target region.
template <isa Isa, unsigned Width> word<Width> stripe_loop(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept {
  return stripe_loop_any<Isa, Width>(lanes, data, converged);
}

template <isa Isa, unsigned Width> word<Width> run_time_hash(std::span<std::byte const> data, word<Width> seed) noexcept {
  return run_time_hash_any<Isa, Width>(data, seed);
}

template std::uint32_t stripe_loop<isa::CHECKSUM_PRESET, 32>(lane_array<32> &, std::span<std::byte const>, bool) noexcept;
template std::uint64_t stripe_loop<isa::CHECKSUM_PRESET, 64>(lane_array<64> &, std::span<std::byte const>, bool) noexcept;
template std::uint32_t run_time_hash<isa::CHECKSUM_PRESET, 32>(std::span<std::byte const>, std::uint32_t) noexcept;
template std::uint64_t run_time_hash<isa::CHECKSUM_PRESET, 64>(std::span<std::byte const>, std::uint64_t) noexcept;

// Dispatch engines run this preset only where its loop differs from generic's: it has target options or a kernel.
static_assert((xxhash_dispatch_engine<32>(0, isa::CHECKSUM_PRESET).preset() == isa::CHECKSUM_PRESET) ==
              (!isa_detail::without_target_options(isa::CHECKSUM_PRESET)));

} // namespace checksum::xxhash_detail

#endif // CHECKSUM_PRIVATE_XXHASH_LOOPS_HPP
