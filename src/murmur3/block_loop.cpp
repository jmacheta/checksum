// The run-time loops of MurmurHash3, built only for isa::generic: no preset changes them, so every preset runs these.

#include <checksum/byte_range.hpp>
#include <checksum/murmur3.hpp>
#include <checksum_private/target.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::murmur3_detail {

// fold_blocks<32>() four blocks per iteration: the scrambles do not depend on the lane, so they run while earlier blocks update the lane.
// NOLINTNEXTLINE(misc-use-internal-linkage): with internal linkage Clang warns that x86-64 does not use it.
CHECKSUM_ALWAYS_INLINE inline lane_array<32> fold_blocks_by_four(lane_array<32> lanes, std::span<std::byte const> data) noexcept;

// fold_blocks_by_four() on 32-bit Arm, whose in-order cores (Cortex-M, Cortex-R, small A-profile) gain from blocks taken four at a
// time; out-of-order cores already overlap the blocks (on Cortex-A72 one or four blocks run as fast). Else fold_blocks<32>().
// NOLINTNEXTLINE(misc-use-internal-linkage): the same linkage as fold_blocks_by_four().
CHECKSUM_ALWAYS_INLINE inline lane_array<32> fold_blocks_32(lane_array<32> lanes, std::span<std::byte const> data) noexcept;

lane_array<32> fold_blocks_by_four(lane_array<32> lanes, std::span<std::byte const> data) noexcept {
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

lane_array<32> fold_blocks_32(lane_array<32> lanes, std::span<std::byte const> data) noexcept {
  if constexpr(target_detail::arm32_target) {
    return fold_blocks_by_four(lanes, data);
  } else {
    return fold_blocks<32>(lanes, data);
  }
}

template <unsigned Width> void block_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  if constexpr(Width == 32) {
    lanes = fold_blocks_32(lanes, data);
  } else {
    lanes = fold_blocks<Width>(lanes, data);
  }
}

std::uint32_t run_time_hash_32(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % block_size<32>);
  return finish<32>(fold_blocks_32(initial_lanes<32>(seed), data.first(whole)), data.size(), data.subspan(whole));
}

template void block_loop<32>(lane_array<32> &, std::span<std::byte const>) noexcept;
template void block_loop<128>(lane_array<128> &, std::span<std::byte const>) noexcept;

} // namespace checksum::murmur3_detail
