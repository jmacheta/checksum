#ifndef CHECKSUM_MURMUR3_HPP
#define CHECKSUM_MURMUR3_HPP

#include <checksum/byte_range.hpp>
#include <checksum/hash128.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_murmur3 MurmurHash3
 *   The non-cryptographic hashes MurmurHash3_x86_32 and MurmurHash3_x64_128 with a seed, at compile time or at run time.
 *   @{
 */

namespace checksum {

/// Running MurmurHash3_x86_32 (Width 32) or MurmurHash3_x64_128 (Width 128) hash. Every value is valid; the default is the empty message
/// with seed 0, and murmur3_state<Width>{.seed = seed} starts one with another seed.
template <unsigned Width>
  requires(Width == 32 || Width == 128)
struct murmur3_state {
  /// The hash: std::uint32_t, or hash128 with low = h1 and high = h2, the 128-bit number the reference implementation stores little-endian.
  using value_type = std::conditional_t<Width == 32, std::uint32_t, hash128>;

  /// Hash lanes: h1, and h2 for Width 128. Set from the seed when the first block is folded.
  std::array<std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>, Width == 32 ? 1 : 2> lanes{};
  std::uint64_t length = 0;                  ///< Bytes folded so far.
  std::uint32_t seed = 0;                    ///< Seed of the hash.
  std::array<std::byte, Width / 8> buffer{}; ///< The unfinished block: its first length % buffer.size() bytes.
};

/// Folds data into state; data may be split anywhere.
template <unsigned Width>
[[nodiscard]] constexpr murmur3_state<Width> murmur3_update(murmur3_state<Width> state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <unsigned Width, byte_range Range>
[[nodiscard]] constexpr murmur3_state<Width> murmur3_update(murmur3_state<Width> state, Range &&data) noexcept;

/// The hash of the message folded into state. Messages of 4 GiB or more mix in their length modulo 2^32 for Width 32.
template <unsigned Width> [[nodiscard]] constexpr murmur3_state<Width>::value_type murmur3_finalize(murmur3_state<Width> const &state) noexcept;

/// The hash of data: murmur3_finalize(murmur3_update(murmur3_state<Width>{.seed = seed}, data)).
template <unsigned Width>
[[nodiscard]] constexpr murmur3_state<Width>::value_type murmur3_compute(std::span<std::byte const> data, std::uint32_t seed = 0) noexcept;

/// The hash of a byte range with a seed. A std::span<std::byte const> takes the overload above.
template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
[[nodiscard]] constexpr murmur3_state<Width>::value_type murmur3_compute(Range &&data, std::uint32_t seed = 0) noexcept;

/// Running MurmurHash3_x86_32 hash.
using murmur3_32_state = murmur3_state<32>;

/// Running MurmurHash3_x64_128 hash.
using murmur3_128_state = murmur3_state<128>;

/// The MurmurHash3_x86_32 hash of data: murmur3_compute<32>(data, seed).
template <byte_range Range> [[nodiscard]] constexpr murmur3_32_state::value_type murmur3_32_compute(Range &&data, std::uint32_t seed = 0) noexcept;

/// The MurmurHash3_x64_128 hash of data: murmur3_compute<128>(data, seed).
template <byte_range Range> [[nodiscard]] constexpr murmur3_128_state::value_type murmur3_128_compute(Range &&data, std::uint32_t seed = 0) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::murmur3_detail {

template <unsigned Width> using lane_array = decltype(murmur3_state<Width>::lanes);

template <unsigned Width> using lane = lane_array<Width>::value_type;

template <unsigned Width> inline constexpr std::size_t block_size = Width / 8;

// The multipliers, rotations and addends of each width; multiplier_1 and multiplier_2 are c1 and c2 of the reference.
template <unsigned Width> struct algorithm_constants;

template <> struct algorithm_constants<32> {
  static constexpr std::uint32_t multiplier_1 = 0xCC9E2D51U;
  static constexpr std::uint32_t multiplier_2 = 0x1B873593U;
  static constexpr int word_rotation = 15;
  static constexpr int lane_rotation = 13;
  static constexpr std::uint32_t lane_multiplier = 5;
  static constexpr std::uint32_t lane_addend = 0xE6546B64U;
  static constexpr std::array<std::uint32_t, 2> avalanche_multipliers{0x85EBCA6BU, 0xC2B2AE35U};
  static constexpr std::array<int, 3> avalanche_shifts{16, 13, 16};
};

template <> struct algorithm_constants<128> {
  static constexpr std::uint64_t multiplier_1 = 0x87C37B91114253D5U;
  static constexpr std::uint64_t multiplier_2 = 0x4CF5AD432745937FU;
  static constexpr std::array<int, 2> word_rotations{31, 33};
  static constexpr std::array<int, 2> lane_rotations{27, 31};
  static constexpr std::uint64_t lane_multiplier = 5;
  static constexpr std::array<std::uint64_t, 2> lane_addends{0x52DCE729U, 0x38495AB5U};
  static constexpr std::array<std::uint64_t, 2> avalanche_multipliers{0xFF51AFD7ED558CCDU, 0xC4CEB9FE1A85EC53U};
  static constexpr std::array<int, 3> avalanche_shifts{33, 33, 33};
};

using detail::load;

// Reads a little-endian integer from the first size bytes at data, size < sizeof(Integer), with fixed-size loads.
template <class Integer> constexpr Integer load_partial(std::byte const *data, std::size_t size) noexcept;

// Scrambles one input word before it is mixed into a lane: the low word of a block (half 0) or the high word (half 1).
template <unsigned Width> constexpr lane<Width> scramble(lane<Width> word, std::size_t half) noexcept;

// Mixes all bits of a lane into each other.
template <unsigned Width> constexpr lane<Width> avalanche(lane<Width> value) noexcept;

template <unsigned Width> constexpr lane_array<Width> initial_lanes(std::uint32_t seed) noexcept;

// Folds the whole blocks of data into lanes.
template <unsigned Width> constexpr lane_array<Width> fold_blocks(lane_array<Width> lanes, std::span<std::byte const> data) noexcept;

// fold_blocks(); defined in src/murmur3/block_loop.cpp for both widths. Lanes by reference keep the array out of the stack arguments
// and the return slot.
template <unsigned Width> void block_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// x64_128 inputs shorter than this fold inline: the call costs up to 40 % at 20 bytes on Cortex-A72 and Cortex-M4. Inlining longer
// ones costs 8 % on AArch32, and inlining x86_32 lost up to 11 % on Cortex-M4.
inline constexpr std::size_t out_of_line_size = 256;

// fold_blocks() during constant evaluation or for short x64_128 inputs, else block_loop().
template <unsigned Width> constexpr void fold(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// The hash of a message of length bytes: lanes hold its whole blocks, tail holds the rest.
template <unsigned Width>
constexpr murmur3_state<Width>::value_type finish(lane_array<Width> lanes, std::uint64_t length, std::span<std::byte const> tail) noexcept;

} // namespace checksum::murmur3_detail

///@}
///@}

namespace checksum::murmur3_detail {

template <class Integer> constexpr Integer load_partial(std::byte const *data, std::size_t size) noexcept {
  Integer value = 0;
  std::size_t offset = 0;
  if constexpr(sizeof(Integer) == 8) {
    if((size & 4U) != 0) {
      value = load<std::uint32_t>(data);
      offset = 4;
    }
  }
  if((size & 2U) != 0) {
    value |= Integer{load<std::uint16_t>(data + offset)} << (8 * offset);
    offset += 2;
  }
  if((size & 1U) != 0) {
    value |= std::to_integer<Integer>(data[offset]) << (8 * offset);
  }
  return value;
}

template <unsigned Width> constexpr lane<Width> scramble(lane<Width> word, std::size_t half) noexcept {
  using constants = algorithm_constants<Width>;
  if constexpr(Width == 32) {
    return std::rotl(static_cast<lane<Width>>(word * constants::multiplier_1), constants::word_rotation) * constants::multiplier_2;
  } else {
    lane<Width> const first = half == 0 ? constants::multiplier_1 : constants::multiplier_2;
    lane<Width> const second = half == 0 ? constants::multiplier_2 : constants::multiplier_1;
    return std::rotl(word * first, constants::word_rotations[half]) * second;
  }
}

template <unsigned Width> constexpr lane<Width> avalanche(lane<Width> value) noexcept {
  using constants = algorithm_constants<Width>;
  value ^= value >> constants::avalanche_shifts[0];
  value *= constants::avalanche_multipliers[0];
  value ^= value >> constants::avalanche_shifts[1];
  value *= constants::avalanche_multipliers[1];
  value ^= value >> constants::avalanche_shifts[2];
  return value;
}

template <unsigned Width> constexpr lane_array<Width> initial_lanes(std::uint32_t seed) noexcept {
  lane_array<Width> lanes{};
  lanes.fill(seed);
  return lanes;
}

template <unsigned Width> constexpr lane_array<Width> fold_blocks(lane_array<Width> lanes, std::span<std::byte const> data) noexcept {
  using constants = algorithm_constants<Width>;
  std::byte const *position = data.data();
  for(std::size_t size = data.size(); size >= block_size<Width>; size -= block_size<Width>, position += block_size<Width>) {
    if constexpr(Width == 32) {
      lanes[0] = (std::rotl(lanes[0] ^ scramble<Width>(load<std::uint32_t>(position), 0), constants::lane_rotation) * constants::lane_multiplier) +
                 constants::lane_addend;
    } else {
      lanes[0] = std::rotl(lanes[0] ^ scramble<Width>(load<std::uint64_t>(position), 0), constants::lane_rotations[0]) + lanes[1];
      lanes[0] = (lanes[0] * constants::lane_multiplier) + constants::lane_addends[0];
      lanes[1] = std::rotl(lanes[1] ^ scramble<Width>(load<std::uint64_t>(position + 8), 1), constants::lane_rotations[1]) + lanes[0];
      lanes[1] = (lanes[1] * constants::lane_multiplier) + constants::lane_addends[1];
    }
  }
  return lanes;
}

template <unsigned Width> constexpr void fold(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  if consteval {
    lanes = fold_blocks<Width>(lanes, data);
  } else {
    if(Width == 128 && data.size() < out_of_line_size) {
      lanes = fold_blocks<Width>(lanes, data);
    } else {
      block_loop<Width>(lanes, data);
    }
  }
}

template <unsigned Width>
constexpr murmur3_state<Width>::value_type finish(lane_array<Width> lanes, std::uint64_t length, std::span<std::byte const> tail) noexcept {
  // The tail is scrambled as a block padded with zeros; scrambling a zero word gives zero, so absent words change nothing.
  if constexpr(Width == 32) {
    return avalanche<Width>(lanes[0] ^ scramble<Width>(load_partial<std::uint32_t>(tail.data(), tail.size()), 0) ^
                            static_cast<std::uint32_t>(length));
  } else {
    bool const two_words = tail.size() >= 8;
    std::uint64_t const low_word = two_words ? load<std::uint64_t>(tail.data()) : load_partial<std::uint64_t>(tail.data(), tail.size());
    std::uint64_t const high_word = two_words ? load_partial<std::uint64_t>(tail.data() + 8, tail.size() - 8) : 0;
    std::uint64_t low = lanes[0] ^ scramble<Width>(low_word, 0) ^ length;
    std::uint64_t high = lanes[1] ^ scramble<Width>(high_word, 1) ^ length;
    low += high;
    high += low;
    low = avalanche<Width>(low);
    high = avalanche<Width>(high);
    low += high;
    high += low;
    return {.low = low, .high = high};
  }
}

} // namespace checksum::murmur3_detail

namespace checksum {

template <unsigned Width> constexpr murmur3_state<Width> murmur3_update(murmur3_state<Width> state, std::span<std::byte const> data) noexcept {
  constexpr std::size_t block_size = murmur3_detail::block_size<Width>;
  auto const buffered = static_cast<std::size_t>(state.length % block_size);
  if(state.length < block_size && data.size() >= block_size - buffered) {
    state.lanes = murmur3_detail::initial_lanes<Width>(state.seed);
  }
  state.length += data.size();
  if(buffered != 0) {
    std::size_t const fill = std::min(block_size - buffered, data.size());
    std::ranges::copy(data.first(fill), state.buffer.begin() + static_cast<std::ptrdiff_t>(buffered));
    data = data.subspan(fill);
    if(buffered + fill < block_size) {
      return state;
    }
    state.lanes = murmur3_detail::fold_blocks<Width>(state.lanes, state.buffer);
  }
  std::size_t const whole = data.size() - (data.size() % block_size);
  if(whole != 0) {
    murmur3_detail::fold<Width>(state.lanes, data.first(whole));
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
  return state;
}

template <unsigned Width, byte_range Range> constexpr murmur3_state<Width> murmur3_update(murmur3_state<Width> state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = murmur3_update(state, chunk); });
  return state;
}

template <unsigned Width> constexpr murmur3_state<Width>::value_type murmur3_finalize(murmur3_state<Width> const &state) noexcept {
  constexpr std::size_t block_size = murmur3_detail::block_size<Width>;
  auto const buffered = static_cast<std::size_t>(state.length % block_size);
  auto const lanes = state.length < block_size ? murmur3_detail::initial_lanes<Width>(state.seed) : state.lanes;
  return murmur3_detail::finish<Width>(lanes, state.length, std::span(state.buffer).first(buffered));
}

template <unsigned Width> constexpr murmur3_state<Width>::value_type murmur3_compute(std::span<std::byte const> data, std::uint32_t seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % murmur3_detail::block_size<Width>);
  murmur3_detail::lane_array<Width> lanes = murmur3_detail::initial_lanes<Width>(seed);
  if(whole != 0) {
    murmur3_detail::fold<Width>(lanes, data.first(whole));
  }
  return murmur3_detail::finish<Width>(lanes, data.size(), data.subspan(whole));
}

template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
constexpr murmur3_state<Width>::value_type murmur3_compute(Range &&data, std::uint32_t seed) noexcept {
  // Contiguous ranges skip the buffer of the state.
  if constexpr(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>) {
    if !consteval {
      return murmur3_compute<Width>(std::as_bytes(std::span(std::ranges::data(data), std::ranges::size(data))), seed);
    }
  }
  return murmur3_finalize(murmur3_update(murmur3_state<Width>{.seed = seed}, std::forward<Range>(data)));
}

template <byte_range Range> constexpr murmur3_32_state::value_type murmur3_32_compute(Range &&data, std::uint32_t seed) noexcept {
  return murmur3_compute<32>(std::forward<Range>(data), seed);
}

template <byte_range Range> constexpr murmur3_128_state::value_type murmur3_128_compute(Range &&data, std::uint32_t seed) noexcept {
  return murmur3_compute<128>(std::forward<Range>(data), seed);
}

} // namespace checksum

#endif // CHECKSUM_MURMUR3_HPP
