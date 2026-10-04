// Run-time loop of the fletcher4 checksum.

#include <checksum/fletcher4.hpp>
#include <checksum_private/fletcher4_arch.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::fletcher4_detail {

namespace {

// Bytes of one word per lane.
inline constexpr std::size_t group_size = lane_count * word_size;

// Shortest input for the lanes. The portable ones overtake the word loop on a Cortex-A72 at 256 bytes with GCC, 384 with Clang; on 32-bit targets
// their 64-bit sums run out of registers, 3 times slower there.
inline constexpr std::size_t lanes_minimum_size =
    lane_kernel_available ? lane_kernel_minimum_size
                          : (std::numeric_limits<std::size_t>::digits == 64 ? 256 : std::numeric_limits<std::size_t>::max());

// The little-endian word at data, from byte loads that compilers merge where unaligned loads are allowed: no memcpy call elsewhere.
std::uint64_t load(std::byte const *data) noexcept;

// The portable lanes over groups groups of words at data.
[[gnu::always_inline, maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept;

// lanes[1] + 2 * lanes[2] + 3 * lanes[3], a weight that grows by one per lane.
[[maybe_unused]] std::uint64_t ramp(std::array<std::uint64_t, lane_count> const &lanes) noexcept;

// The state after the words that lanes summed, a multiple of lane_count; the earlier sums add to the later ones once per word.
[[gnu::always_inline, maybe_unused]] inline fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept;

// One word at a time, then the bytes of an unfinished word.
[[gnu::always_inline]] inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The lanes of the CPU kernel, else the portable ones, then sum_words() over what they left.
[[gnu::always_inline, maybe_unused]] inline fletcher4_state sum_lanes_then_words(fletcher4_state state, std::span<std::byte const> data) noexcept;

// sum_lanes_then_words(). Not inlined, so that short inputs run sum_loop() without a call.
[[gnu::noinline, maybe_unused]] fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept;

// compute_loop() from lanes_minimum_size bytes, into value. The empty state is built here, so the short path of compute_loop() stores
// none; and value is the caller's return value, where a returned array would be a local copy behind a stack protector check.
[[gnu::noinline, maybe_unused]] void compute_long(std::span<std::byte const> data, fletcher4_value &value) noexcept;

// Whether data is long enough for the lanes.
[[gnu::always_inline]] inline bool use_lanes(std::span<std::byte const> data) noexcept;

std::uint64_t load(std::byte const *data) noexcept {
  return std::to_integer<std::uint32_t>(data[0]) | (std::to_integer<std::uint32_t>(data[1]) << 8) | (std::to_integer<std::uint32_t>(data[2]) << 16) |
         (std::to_integer<std::uint32_t>(data[3]) << 24);
}

[[gnu::always_inline, maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept {
  lane_sums lanes{};
  for(; groups != 0; --groups, data += group_size) {
    for(std::size_t lane = 0; lane < lane_count; ++lane) {
      lanes[0][lane] += load(data + (lane * word_size));
      lanes[1][lane] += lanes[0][lane];
      lanes[2][lane] += lanes[1][lane];
      lanes[3][lane] += lanes[2][lane];
    }
  }
  return lanes;
}

std::uint64_t ramp(std::array<std::uint64_t, lane_count> const &lanes) noexcept { return lanes[1] + (2 * lanes[2]) + (3 * lanes[3]); }

[[gnu::always_inline]] inline fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept {
  // C(n+1, 2) and C(n+2, 3) modulo 2^64 for an even n; the second divides exactly by 3, a product with its inverse modulo 2^64.
  std::uint64_t const triangular = (words / 2) * (words + 1);
  std::uint64_t const tetrahedral = triangular * (words + 2) * 0xAAAA'AAAA'AAAA'AAABU;
  auto const total = [&](std::size_t source) { return lanes[source][0] + lanes[source][1] + lanes[source][2] + lanes[source][3]; };
  // Word i of n weighs 1, n-i, C(n-i+1, 2) and C(n-i+2, 3) in sum1 to sum4; these are those weights in terms of the lane sums.
  std::uint64_t const tail1 = lanes[0][2] + (3 * lanes[0][3]);
  std::uint64_t const tail2 = lanes[1][2] + (3 * lanes[1][3]);
  return {.sum1 = state.sum1 + total(0),
          .sum2 = state.sum2 + (words * state.sum1) + (4 * total(1)) - ramp(lanes[0]),
          .sum3 = state.sum3 + (words * state.sum2) + (triangular * state.sum1) + (16 * total(2)) - (6 * total(1)) - (4 * ramp(lanes[1])) + tail1,
          .sum4 = state.sum4 + (words * state.sum3) + (triangular * state.sum2) + (tetrahedral * state.sum1) + (64 * total(3)) - (48 * total(2)) -
                  (16 * ramp(lanes[2])) + (4 * total(1)) + (6 * ramp(lanes[1])) + (4 * tail2) - lanes[0][3]};
}

[[gnu::always_inline]] inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::byte const *position = data.data();
  for(std::size_t words = data.size() / word_size; words != 0; --words, position += word_size) {
    state.sum1 += load(position);
    finish_word(state);
  }
  // The bytes of an unfinished word, at most 3, go to sum1 only.
  std::size_t const tail = data.size() % word_size;
  for(std::size_t index = 0; index < tail; ++index) {
    state.sum1 += std::to_integer<std::uint64_t>(position[index]) << (8 * index);
  }
  state.word_offset = static_cast<std::uint8_t>(tail);
  return state;
}

[[gnu::always_inline]] inline fletcher4_state sum_lanes_then_words(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::size_t const groups = data.size() / group_size;
  if constexpr(lane_kernel_available) {
    state = combine(state, lane_kernel(data.data(), groups), groups * lane_count);
  } else {
    state = combine(state, sum_lanes(data.data(), groups), groups * lane_count);
  }
  return sum_words(state, data.subspan(groups * group_size));
}

[[gnu::noinline]] fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return sum_lanes_then_words(state, data);
}

[[gnu::noinline]] void compute_long(std::span<std::byte const> data, fletcher4_value &value) noexcept {
  value = fletcher4_finalize(sum_lanes_then_words(fletcher4_state{}, data));
}

[[gnu::always_inline]] inline bool use_lanes(std::span<std::byte const> data) noexcept {
  // Without lanes, the long functions are not even linked.
  return lanes_minimum_size != std::numeric_limits<std::size_t>::max() && data.size() >= lanes_minimum_size;
}

} // namespace

fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return use_lanes(data) ? sum_long(state, data) : sum_words(state, data);
}

fletcher4_value compute_loop(std::span<std::byte const> data) noexcept {
  if(use_lanes(data)) {
    fletcher4_value value{};
    compute_long(data, value);
    return value;
  }
  return fletcher4_finalize(sum_words(fletcher4_state{}, data));
}

} // namespace checksum::fletcher4_detail
