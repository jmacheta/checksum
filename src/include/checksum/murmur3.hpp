#ifndef CHECKSUM_MURMUR3_HPP
#define CHECKSUM_MURMUR3_HPP

#include <checksum/byte_range.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
  /// The hash: std::uint32_t, or the two 64-bit halves h1 and h2 in the order the reference implementation stores them.
  using value_type = std::conditional_t<Width == 32, std::uint32_t, std::array<std::uint64_t, 2>>;

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

// Reads a little-endian integer from the first sizeof(Integer) bytes at data.
template <class Integer> constexpr Integer load(std::byte const *data) noexcept;

// Scrambles one input word before it is mixed into a lane: the low word of a block (half 0) or the high word (half 1).
template <unsigned Width> constexpr lane<Width> scramble(lane<Width> word, std::size_t half) noexcept;

// Mixes all bits of a lane into each other.
template <unsigned Width> constexpr lane<Width> avalanche(lane<Width> value) noexcept;

template <unsigned Width> constexpr lane_array<Width> initial_lanes(std::uint32_t seed) noexcept;

// Folds the whole blocks of data into lanes.
template <unsigned Width> constexpr lane_array<Width> fold_blocks(lane_array<Width> lanes, std::span<std::byte const> data) noexcept;

// fold_blocks(); defined in src/murmur3/block_loop.cpp for both widths.
template <unsigned Width> lane_array<Width> block_loop(lane_array<Width> lanes, std::span<std::byte const> data) noexcept;

// fold_blocks() during constant evaluation, else block_loop().
template <unsigned Width> constexpr lane_array<Width> fold(lane_array<Width> lanes, std::span<std::byte const> data) noexcept;

// The hash of a message of length bytes: lanes hold its whole blocks, tail holds the rest.
template <unsigned Width>
constexpr murmur3_state<Width>::value_type finish(lane_array<Width> lanes, std::uint64_t length, std::span<std::byte const> tail) noexcept;

} // namespace checksum::murmur3_detail

///@}
///@}

namespace checksum::murmur3_detail {

template <class Integer> constexpr Integer load(std::byte const *data) noexcept {
  Integer value = 0;
  if consteval {
    for(std::size_t index = 0; index < sizeof(Integer); ++index) {
      value |= std::to_integer<Integer>(data[index]) << (8 * index);
    }
  } else {
    std::memcpy(&value, data, sizeof(Integer));
    if constexpr(std::endian::native == std::endian::big) {
      value = std::byteswap(value);
    }
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

template <unsigned Width> constexpr lane_array<Width> fold(lane_array<Width> lanes, std::span<std::byte const> data) noexcept {
  if consteval {
    return fold_blocks<Width>(lanes, data);
  } else {
    return block_loop<Width>(lanes, data);
  }
}

template <unsigned Width>
constexpr murmur3_state<Width>::value_type finish(lane_array<Width> lanes, std::uint64_t length, std::span<std::byte const> tail) noexcept {
  // The tail is scrambled as a block padded with zeros; scrambling a zero word gives zero, so absent words change nothing.
  lane_array<Width> words{};
  for(std::size_t index = 0; index < tail.size(); ++index) {
    words[index / sizeof(lane<Width>)] |= std::to_integer<lane<Width>>(tail[index]) << (8 * (index % sizeof(lane<Width>)));
  }
  if constexpr(Width == 32) {
    return avalanche<Width>(lanes[0] ^ scramble<Width>(words[0], 0) ^ static_cast<std::uint32_t>(length));
  } else {
    std::uint64_t low = lanes[0] ^ scramble<Width>(words[0], 0) ^ length;
    std::uint64_t high = lanes[1] ^ scramble<Width>(words[1], 1) ^ length;
    low += high;
    high += low;
    low = avalanche<Width>(low);
    high = avalanche<Width>(high);
    low += high;
    high += low;
    return {low, high};
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
    state.lanes = murmur3_detail::fold<Width>(state.lanes, data.first(whole));
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
    lanes = murmur3_detail::fold<Width>(lanes, data.first(whole));
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

} // namespace checksum

#endif // CHECKSUM_MURMUR3_HPP
