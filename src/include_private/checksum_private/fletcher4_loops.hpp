#ifndef CHECKSUM_PRIVATE_FLETCHER4_LOOPS_HPP
#define CHECKSUM_PRIVATE_FLETCHER4_LOOPS_HPP

// The run-time loops of the fletcher4 checksum for one preset: the portable word and lane loops, the CPU lane kernel of
// fletcher4_arch.hpp and the choice between them. A source of src/fletcher4/ defines CHECKSUM_PRESET (see target_begin.hpp)
// and includes this header once.

// Every std and public header comes before the target region, so that their inline functions compile for the build's own target.
#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::fletcher4_detail {

// Bytes of one word per lane.
inline constexpr std::size_t group_size = lane_count * word_size;

} // namespace checksum::fletcher4_detail

#include <checksum_private/target_begin.hpp>

#include <checksum_private/fletcher4_arch.hpp>

namespace checksum::fletcher4_detail {

// Shortest input for the lanes. The portable ones overtake the word loop on a Cortex-A72 at 128 bytes with GCC but stay below
// Clang's unrolled word loop; on 32-bit targets their 64-bit sums run out of registers, 3 times slower there.
inline constexpr std::size_t portable_lanes_minimum_size = target_detail::word32_target ? std::numeric_limits<std::size_t>::max() : 256;
template <isa Isa>
inline constexpr std::size_t lanes_minimum_size = lane_kernel<Isa>::available ? lane_kernel<Isa>::minimum_size : portable_lanes_minimum_size;

// The little-endian word at data, from byte loads that compilers merge where unaligned loads are allowed: no memcpy call elsewhere.
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline std::uint64_t load(std::byte const *data) noexcept;

// The portable lanes over groups groups of words at data.
template <isa Isa> CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept;

// lanes[1] + 2 * lanes[2] + 3 * lanes[3], a weight that grows by one per lane.
template <isa Isa> [[maybe_unused]] inline std::uint64_t ramp(std::array<std::uint64_t, lane_count> const &lanes) noexcept;

// The state after the words that lanes summed, a multiple of lane_count; the earlier sums add to the later ones once per word.
template <isa Isa>
CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept;

// One word at a time (two on in-order targets), then the bytes of an unfinished word.
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The lanes of the CPU kernel, else the portable ones, then sum_words() over what they left.
template <isa Isa>
CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline fletcher4_state sum_lanes_then_words(fletcher4_state state, std::span<std::byte const> data) noexcept;

// sum_lanes_then_words(). Not inlined, so that short inputs run the entry without a call.
template <isa Isa> CHECKSUM_NOINLINE fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept;

// compute_loop() from lanes_minimum_size bytes, into value. The empty state is built here, so the short path of compute_loop()
// stores none; and value is the caller's return value, where a returned array would be a local copy behind a stack protector check.
template <isa Isa> CHECKSUM_NOINLINE void compute_long(std::span<std::byte const> data, fletcher4_value &value) noexcept;

// Whether data is long enough for the lanes.
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline bool use_lanes(std::span<std::byte const> data) noexcept;

// The run-time fletcher4 fold for preset Isa; sum_loop() forwards to it.
template <isa Isa> fletcher4_state fletcher4_sum_any(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The run-time fletcher4 compute for preset Isa; compute_loop() forwards to it.
template <isa Isa> fletcher4_value fletcher4_compute_any(std::span<std::byte const> data) noexcept;

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline std::uint64_t load(std::byte const *data) noexcept {
  // NOLINTNEXTLINE(readability-magic-numbers): the bit offsets of the bytes.
  return std::to_integer<std::uint32_t>(data[0]) | (std::to_integer<std::uint32_t>(data[1]) << 8U) |
         (std::to_integer<std::uint32_t>(data[2]) << 16U) | (std::to_integer<std::uint32_t>(data[3]) << 24U); // NOLINT(readability-magic-numbers)
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept {
  lane_sums lanes{};
  for(; groups != 0; --groups, data += group_size) {
    for(std::size_t lane = 0; lane < lane_count; ++lane) {
      lanes[0][lane] += load<Isa>(data + (lane * word_size));
      lanes[1][lane] += lanes[0][lane];
      lanes[2][lane] += lanes[1][lane];
      lanes[3][lane] += lanes[2][lane];
    }
  }
  return lanes;
}

template <isa Isa> [[maybe_unused]] inline std::uint64_t ramp(std::array<std::uint64_t, lane_count> const &lanes) noexcept {
  return lanes[1] + (2 * lanes[2]) + (3 * lanes[3]);
}

template <isa Isa>
CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept {
  // C(n+1, 2) and C(n+2, 3) modulo 2^64 for an even n; the second divides exactly by 3, a product with its inverse modulo 2^64.
  std::uint64_t const triangular = (words / 2) * (words + 1);
  std::uint64_t const tetrahedral = triangular * (words + 2) * 0xAAAA'AAAA'AAAA'AAABU;
  auto const total = [&](std::size_t source) { return lanes[source][0] + lanes[source][1] + lanes[source][2] + lanes[source][3]; };
  // Word i of n weighs 1, n-i, C(n-i+1, 2) and C(n-i+2, 3) in sum1 to sum4; these are those weights in terms of the lane sums, where
  // a lane word stands for lane_count message words.
  constexpr std::uint64_t width = lane_count;
  constexpr std::uint64_t squared = width * width;
  constexpr std::uint64_t cubed = squared * width;
  constexpr std::uint64_t pairs = (squared - width) / 2;
  std::uint64_t const tail1 = lanes[0][2] + (3 * lanes[0][3]);
  std::uint64_t const tail2 = lanes[1][2] + (3 * lanes[1][3]);
  return {.sum1 = state.sum1 + total(0),
          .sum2 = state.sum2 + (words * state.sum1) + (width * total(1)) - ramp<Isa>(lanes[0]),
          .sum3 = state.sum3 + (words * state.sum2) + (triangular * state.sum1) + (squared * total(2)) - (pairs * total(1)) -
                  (width * ramp<Isa>(lanes[1])) + tail1,
          .sum4 = state.sum4 + (words * state.sum3) + (triangular * state.sum2) + (tetrahedral * state.sum1) + (cubed * total(3)) -
                  ((cubed - squared) * total(2)) - (squared * ramp<Isa>(lanes[2])) + (width * total(1)) + (pairs * ramp<Isa>(lanes[1])) +
                  (width * tail2) - lanes[0][3]};
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::byte const *position = data.data();
  std::size_t words = data.size() / word_size;
  // In order, two words per iteration halve the loop overhead around the carry chains.
  if constexpr(target_detail::in_order_target<Isa>) {
    for(std::size_t pairs = words / 2; pairs != 0; --pairs, position += std::size_t{2} * word_size) {
      std::uint64_t const first = load<Isa>(position);
      std::uint64_t const second = load<Isa>(position + word_size);
      state.sum1 += first;
      finish_word(state);
      state.sum1 += second;
      finish_word(state);
    }
    words %= 2;
  }
  for(; words != 0; --words, position += word_size) {
    state.sum1 += load<Isa>(position);
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

template <isa Isa>
CHECKSUM_ALWAYS_INLINE [[maybe_unused]] inline fletcher4_state sum_lanes_then_words(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::size_t const groups = data.size() / group_size;
  if constexpr(lane_kernel<Isa>::available) {
    state = combine<Isa>(state, lane_kernel<Isa>::sum(data.data(), groups), groups * lane_count);
  } else {
    state = combine<Isa>(state, sum_lanes<Isa>(data.data(), groups), groups * lane_count);
  }
  return sum_words<Isa>(state, data.subspan(groups * group_size));
}

template <isa Isa> CHECKSUM_NOINLINE fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return sum_lanes_then_words<Isa>(state, data);
}

template <isa Isa> CHECKSUM_NOINLINE void compute_long(std::span<std::byte const> data, fletcher4_value &value) noexcept {
  value = finish(sum_lanes_then_words<Isa>(fletcher4_state{}, data));
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline bool use_lanes(std::span<std::byte const> data) noexcept {
  // Without lanes, the long functions are not even linked.
  return lanes_minimum_size<Isa> != std::numeric_limits<std::size_t>::max() && data.size() >= lanes_minimum_size<Isa>;
}

template <isa Isa> fletcher4_state fletcher4_sum_any(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return use_lanes<Isa>(data) ? sum_long<Isa>(state, data) : sum_words<Isa>(state, data);
}

template <isa Isa> fletcher4_value fletcher4_compute_any(std::span<std::byte const> data) noexcept {
  if(use_lanes<Isa>(data)) {
    fletcher4_value value{};
    compute_long<Isa>(data, value);
    return value;
  }
  return finish(sum_words<Isa>(fletcher4_state{}, data));
}

} // namespace checksum::fletcher4_detail

#include <checksum_private/target_end.hpp>

namespace checksum::fletcher4_detail {

// GCC fixes a function's target at its first declaration, which for sum_loop() and compute_loop() is in fletcher4.hpp: they
// only forward to functions first declared inside the target region.
template <isa Isa> fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return fletcher4_sum_any<Isa>(state, data);
}

template <isa Isa> fletcher4_value compute_loop(std::span<std::byte const> data) noexcept { return fletcher4_compute_any<Isa>(data); }

template fletcher4_state sum_loop<isa::CHECKSUM_PRESET>(fletcher4_state state, std::span<std::byte const> data) noexcept;
template fletcher4_value compute_loop<isa::CHECKSUM_PRESET>(std::span<std::byte const> data) noexcept;

// The dispatch engines choose this preset exactly where its loop differs from generic's: it has target options or a kernel.
static_assert(own_loops.contains(isa::CHECKSUM_PRESET) ==
              (!isa_detail::without_target_options(isa::CHECKSUM_PRESET) || lane_kernel<isa::CHECKSUM_PRESET>::available));

} // namespace checksum::fletcher4_detail

#endif // CHECKSUM_PRIVATE_FLETCHER4_LOOPS_HPP
