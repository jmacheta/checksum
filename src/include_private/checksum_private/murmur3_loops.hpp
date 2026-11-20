#ifndef CHECKSUM_PRIVATE_MURMUR3_LOOPS_HPP
#define CHECKSUM_PRIVATE_MURMUR3_LOOPS_HPP

// Run-time block loop of MurmurHash3 for one preset: the portable loops of murmur3.hpp and the 32-bit Arm grouping. A source of
// src/murmur3/ defines CHECKSUM_PRESET (see target_begin.hpp) and includes this header once.

// Every std header of this file comes before the target region, so that their inline functions compile for the build's own target.
#include <checksum/isa.hpp>
#include <checksum/murmur3.hpp>
#include <checksum_private/target.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include <checksum_private/target_begin.hpp>

namespace checksum::murmur3_detail {

// fold_blocks<32>() four blocks per iteration: the scrambles do not depend on the lane, so they run while earlier blocks update the lane.
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline lane_array<32> fold_blocks_by_four(lane_array<32> lanes, std::span<std::byte const> data) noexcept;

// fold_blocks_by_four() on in-order targets, which gain from blocks taken four at a time; out-of-order cores already overlap
// the blocks. Else fold_blocks<32>().
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline lane_array<32> fold_blocks_32(lane_array<32> lanes, std::span<std::byte const> data) noexcept;

// The run-time block loop for preset Isa; block_loop() forwards to it.
template <isa Isa, unsigned Width> void block_loop_any(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// The run-time x86_32 hash for preset Isa; run_time_hash_32() forwards to it.
template <isa Isa> std::uint32_t run_time_hash_32_any(std::span<std::byte const> data, std::uint32_t seed) noexcept;

template <isa Isa> lane_array<32> fold_blocks_by_four(lane_array<32> lanes, std::span<std::byte const> data) noexcept {
  using constants = algorithm_constants<32>;
  constexpr std::size_t size = block_size<32>;
  std::uint32_t lane = lanes[0];
  auto const mix = [&lane](std::uint32_t key) {
    lane = (std::rotl(lane ^ key, constants::lane_rotation) * constants::lane_multiplier) + constants::lane_addend;
  };
  std::byte const *position = data.data();
  for(std::size_t groups = data.size() / (4 * size); groups != 0; --groups, position += 4 * size) {
    std::uint32_t const first = scramble<32>(load<std::uint32_t>(position), 0);
    std::uint32_t const second = scramble<32>(load<std::uint32_t>(position + size), 0);
    std::uint32_t const third = scramble<32>(load<std::uint32_t>(position + (2 * size)), 0);
    std::uint32_t const fourth = scramble<32>(load<std::uint32_t>(position + (3 * size)), 0);
    mix(first);
    mix(second);
    mix(third);
    mix(fourth);
  }
  for(std::size_t rest = (data.size() / size) % 4; rest != 0; --rest, position += size) {
    mix(scramble<32>(load<std::uint32_t>(position), 0));
  }
  lanes[0] = lane;
  return lanes;
}

template <isa Isa> lane_array<32> fold_blocks_32(lane_array<32> lanes, std::span<std::byte const> data) noexcept {
  return target_detail::in_order_target<Isa> ? fold_blocks_by_four<Isa>(lanes, data) : fold_blocks<32>(lanes, data);
}

template <isa Isa, unsigned Width> void block_loop_any(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  if constexpr(Width == 32) {
    lanes = fold_blocks_32<Isa>(lanes, data);
  } else {
    lanes = fold_blocks<Width>(lanes, data);
  }
}

template <isa Isa> std::uint32_t run_time_hash_32_any(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % block_size<32>);
  return finish<32>(fold_blocks_32<Isa>(initial_lanes<32>(seed), data.first(whole)), data.size(), data.subspan(whole));
}

} // namespace checksum::murmur3_detail

#include <checksum_private/target_end.hpp>

namespace checksum::murmur3_detail {

// GCC fixes a function's target at its first declaration, which for block_loop() is in murmur3.hpp: it only forwards to block_loop_any(),
// first declared inside the target region.
template <isa Isa, unsigned Width> void block_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  block_loop_any<Isa, Width>(lanes, data);
}

template <isa Isa> std::uint32_t run_time_hash_32(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  return run_time_hash_32_any<Isa>(data, seed);
}

template void block_loop<isa::CHECKSUM_PRESET, 32>(lane_array<32> &, std::span<std::byte const>) noexcept;
template void block_loop<isa::CHECKSUM_PRESET, 128>(lane_array<128> &, std::span<std::byte const>) noexcept;
template std::uint32_t run_time_hash_32<isa::CHECKSUM_PRESET>(std::span<std::byte const>, std::uint32_t) noexcept;

} // namespace checksum::murmur3_detail

#endif // CHECKSUM_PRIVATE_MURMUR3_LOOPS_HPP
