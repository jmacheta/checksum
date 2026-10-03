#ifndef CHECKSUM_XXHASH_HPP
#define CHECKSUM_XXHASH_HPP

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
 *   @defgroup checksum_xxhash xxHash
 *   The non-cryptographic hashes XXH32 and XXH64 with a seed, at compile time or at run time.
 *   @{
 */

namespace checksum {

/// Running XXH32 (Width 32) or XXH64 (Width 64) hash. Every value is valid; the default is the empty message with seed 0, and
/// xxhash_state<Width>{.seed = seed} starts one with another seed.
template <unsigned Width>
  requires(Width == 32 || Width == 64)
struct xxhash_state {
  /// The hash, seed and lane type: std::uint32_t or std::uint64_t.
  using value_type = std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>;

  std::array<value_type, 4> lanes{};                      ///< Lane accumulators; set from the seed when the first stripe is folded.
  std::uint64_t length = 0;                               ///< Bytes folded so far.
  value_type seed = 0;                                    ///< Seed of the hash.
  std::array<std::byte, 4 * sizeof(value_type)> buffer{}; ///< The unfinished stripe: its first length % buffer.size() bytes.
};

/// Folds data into state; data may be split anywhere.
template <unsigned Width>
[[nodiscard]] constexpr xxhash_state<Width> xxhash_update(xxhash_state<Width> state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <unsigned Width, byte_range Range>
[[nodiscard]] constexpr xxhash_state<Width> xxhash_update(xxhash_state<Width> state, Range &&data) noexcept;

/// The hash of the message folded into state.
template <unsigned Width> [[nodiscard]] constexpr xxhash_state<Width>::value_type xxhash_finalize(xxhash_state<Width> const &state) noexcept;

/// The hash of data: xxhash_finalize(xxhash_update(xxhash_state<Width>{.seed = seed}, data)).
template <unsigned Width>
[[nodiscard]] constexpr xxhash_state<Width>::value_type xxhash_compute(std::span<std::byte const> data,
                                                                       typename xxhash_state<Width>::value_type seed = 0) noexcept;

/// The hash of a byte range with a seed. A std::span<std::byte const> takes the overload above.
template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
[[nodiscard]] constexpr xxhash_state<Width>::value_type xxhash_compute(Range &&data, typename xxhash_state<Width>::value_type seed = 0) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::xxhash_detail {

template <unsigned Width> using word = xxhash_state<Width>::value_type;

template <unsigned Width> using lane_array = std::array<word<Width>, 4>;

template <unsigned Width> inline constexpr std::size_t stripe_size = 4 * sizeof(word<Width>);

// Rotations of the four lanes when they converge, for both widths.
inline constexpr std::array<int, 4> convergence_rotations{1, 7, 12, 18};

// The multipliers, rotations and shifts of each width. The tail is mixed in words of the lane size, then a 32-bit half word, then
// bytes.
template <unsigned Width> struct algorithm_constants;

template <> struct algorithm_constants<32> {
  static constexpr std::uint32_t prime_1 = 0x9E3779B1U;
  static constexpr std::uint32_t prime_2 = 0x85EBCA77U;
  static constexpr std::uint32_t prime_3 = 0xC2B2AE3DU;
  static constexpr std::uint32_t prime_4 = 0x27D4EB2FU;
  static constexpr std::uint32_t prime_5 = 0x165667B1U;
  static constexpr int round_rotation = 13;
  static constexpr int word_rotation = 17;
  static constexpr int byte_rotation = 11;
  static constexpr std::array<int, 3> avalanche_shifts{15, 13, 16};
};

template <> struct algorithm_constants<64> {
  static constexpr std::uint64_t prime_1 = 0x9E3779B185EBCA87U;
  static constexpr std::uint64_t prime_2 = 0xC2B2AE3D27D4EB4FU;
  static constexpr std::uint64_t prime_3 = 0x165667B19E3779F9U;
  static constexpr std::uint64_t prime_4 = 0x85EBCA77C2B2AE63U;
  static constexpr std::uint64_t prime_5 = 0x27D4EB2F165667C5U;
  static constexpr int round_rotation = 31;
  static constexpr int word_rotation = 27;
  static constexpr int half_word_rotation = 23;
  static constexpr int byte_rotation = 11;
  static constexpr std::array<int, 3> avalanche_shifts{33, 29, 32};
};

// Reads a little-endian integer from the first sizeof(Integer) bytes at data.
template <class Integer> constexpr Integer load(std::byte const *data) noexcept;

// Mixes one input lane into a lane accumulator.
template <unsigned Width> constexpr word<Width> round(word<Width> accumulator, word<Width> lane) noexcept;

template <unsigned Width> constexpr lane_array<Width> initial_lanes(word<Width> seed) noexcept;

// Merges the four lanes into one value. Spelled out: as a loop, GCC reloads the lanes just stored as one vector, which stalls.
template <unsigned Width> constexpr word<Width> converge(lane_array<Width> const &lanes) noexcept;

// Folds the whole stripes of data into lanes. The lanes stay in memory, where they may alias data: that keeps compilers from packing
// them into one vector register, which is slower than scalar code for XXH32.
template <unsigned Width> constexpr void fold_stripes(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// fold_stripes(), then converge(); defined in src/xxhash/stripe_loop.cpp for both widths. Returning the merged value spares the
// caller reloading the lanes right after their stores.
template <unsigned Width> word<Width> stripe_loop(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// fold_stripes() and converge() during constant evaluation, else stripe_loop().
template <unsigned Width> constexpr word<Width> fold_and_converge(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// The hash of a message of length bytes: converged holds its stripes if length reaches a stripe, tail holds the rest.
template <unsigned Width>
constexpr word<Width> finish(word<Width> converged, word<Width> seed, std::uint64_t length, std::span<std::byte const> tail) noexcept;

} // namespace checksum::xxhash_detail

///@}
///@}

namespace checksum::xxhash_detail {

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

template <unsigned Width> constexpr word<Width> round(word<Width> accumulator, word<Width> lane) noexcept {
  using constants = algorithm_constants<Width>;
  return std::rotl(static_cast<word<Width>>(accumulator + (lane * constants::prime_2)), constants::round_rotation) * constants::prime_1;
}

template <unsigned Width> constexpr lane_array<Width> initial_lanes(word<Width> seed) noexcept {
  using constants = algorithm_constants<Width>;
  return {static_cast<word<Width>>(seed + constants::prime_1 + constants::prime_2), static_cast<word<Width>>(seed + constants::prime_2), seed,
          static_cast<word<Width>>(seed - constants::prime_1)};
}

template <unsigned Width> constexpr word<Width> converge(lane_array<Width> const &lanes) noexcept {
  using constants = algorithm_constants<Width>;
  word<Width> hash = std::rotl(lanes[0], convergence_rotations[0]) + std::rotl(lanes[1], convergence_rotations[1]) +
                     std::rotl(lanes[2], convergence_rotations[2]) + std::rotl(lanes[3], convergence_rotations[3]);
  if constexpr(Width == 64) {
    for(word<Width> const lane : lanes) {
      hash = ((hash ^ round<Width>(0, lane)) * constants::prime_1) + constants::prime_4;
    }
  }
  return hash;
}

template <unsigned Width> constexpr void fold_stripes(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  constexpr std::size_t lane_size = sizeof(word<Width>);
  std::byte const *position = data.data();
  for(std::size_t size = data.size(); size >= stripe_size<Width>; size -= stripe_size<Width>, position += stripe_size<Width>) {
    lanes[0] = round<Width>(lanes[0], load<word<Width>>(position));
    lanes[1] = round<Width>(lanes[1], load<word<Width>>(position + lane_size));
    lanes[2] = round<Width>(lanes[2], load<word<Width>>(position + (2 * lane_size)));
    lanes[3] = round<Width>(lanes[3], load<word<Width>>(position + (3 * lane_size)));
  }
}

template <unsigned Width> constexpr word<Width> fold_and_converge(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept {
  if consteval {
    fold_stripes<Width>(lanes, data);
    return converge<Width>(lanes);
  } else {
    return stripe_loop<Width>(lanes, data);
  }
}

template <unsigned Width>
constexpr word<Width> finish(word<Width> converged, word<Width> seed, std::uint64_t length, std::span<std::byte const> tail) noexcept {
  using constants = algorithm_constants<Width>;
  using value_type = word<Width>;
  value_type hash = length >= stripe_size<Width> ? converged : static_cast<value_type>(seed + constants::prime_5);
  hash += static_cast<value_type>(length);
  std::byte const *position = tail.data();
  std::size_t size = tail.size();
  if constexpr(Width == 32) {
    for(; size >= 4; size -= 4, position += 4) {
      hash = std::rotl(static_cast<value_type>(hash + (load<std::uint32_t>(position) * constants::prime_3)), constants::word_rotation) *
             constants::prime_4;
    }
  } else {
    for(; size >= 8; size -= 8, position += 8) {
      hash = (std::rotl(hash ^ round<Width>(0, load<std::uint64_t>(position)), constants::word_rotation) * constants::prime_1) + constants::prime_4;
    }
    if(size >= 4) {
      hash = (std::rotl(hash ^ (load<std::uint32_t>(position) * constants::prime_1), constants::half_word_rotation) * constants::prime_2) +
             constants::prime_3;
      size -= 4;
      position += 4;
    }
  }
  for(; size != 0; --size, ++position) {
    value_type const byte = std::to_integer<value_type>(*position) * constants::prime_5;
    hash = std::rotl(static_cast<value_type>(Width == 32 ? hash + byte : hash ^ byte), constants::byte_rotation) * constants::prime_1;
  }
  hash ^= hash >> constants::avalanche_shifts[0];
  hash *= constants::prime_2;
  hash ^= hash >> constants::avalanche_shifts[1];
  hash *= constants::prime_3;
  hash ^= hash >> constants::avalanche_shifts[2];
  return hash;
}

} // namespace checksum::xxhash_detail

namespace checksum {

template <unsigned Width> constexpr xxhash_state<Width> xxhash_update(xxhash_state<Width> state, std::span<std::byte const> data) noexcept {
  constexpr std::size_t stripe_size = xxhash_detail::stripe_size<Width>;
  auto const buffered = static_cast<std::size_t>(state.length % stripe_size);
  if(state.length < stripe_size && data.size() >= stripe_size - buffered) {
    state.lanes = xxhash_detail::initial_lanes<Width>(state.seed);
  }
  state.length += data.size();
  if(buffered != 0) {
    std::size_t const fill = std::min(stripe_size - buffered, data.size());
    std::ranges::copy(data.first(fill), state.buffer.begin() + static_cast<std::ptrdiff_t>(buffered));
    data = data.subspan(fill);
    if(buffered + fill < stripe_size) {
      return state;
    }
    xxhash_detail::fold_stripes<Width>(state.lanes, state.buffer);
  }
  std::size_t const whole = data.size() - (data.size() % stripe_size);
  if(whole != 0) {
    xxhash_detail::fold_and_converge<Width>(state.lanes, data.first(whole));
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
  return state;
}

template <unsigned Width, byte_range Range> constexpr xxhash_state<Width> xxhash_update(xxhash_state<Width> state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = xxhash_update(state, chunk); });
  return state;
}

template <unsigned Width> constexpr xxhash_state<Width>::value_type xxhash_finalize(xxhash_state<Width> const &state) noexcept {
  auto const buffered = static_cast<std::size_t>(state.length % xxhash_detail::stripe_size<Width>);
  return xxhash_detail::finish<Width>(xxhash_detail::converge<Width>(state.lanes), state.seed, state.length, std::span(state.buffer).first(buffered));
}

template <unsigned Width>
constexpr xxhash_state<Width>::value_type xxhash_compute(std::span<std::byte const> data, typename xxhash_state<Width>::value_type seed) noexcept {
  std::size_t const whole = data.size() - (data.size() % xxhash_detail::stripe_size<Width>);
  typename xxhash_state<Width>::value_type converged = 0;
  if(whole != 0) {
    xxhash_detail::lane_array<Width> lanes = xxhash_detail::initial_lanes<Width>(seed);
    converged = xxhash_detail::fold_and_converge<Width>(lanes, data.first(whole));
  }
  return xxhash_detail::finish<Width>(converged, seed, data.size(), data.subspan(whole));
}

template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
constexpr xxhash_state<Width>::value_type xxhash_compute(Range &&data, typename xxhash_state<Width>::value_type seed) noexcept {
  // Contiguous ranges skip the buffer of the state.
  if constexpr(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>) {
    if !consteval {
      return xxhash_compute<Width>(std::as_bytes(std::span(std::ranges::data(data), std::ranges::size(data))), seed);
    }
  }
  return xxhash_finalize(xxhash_update(xxhash_state<Width>{.seed = seed}, std::forward<Range>(data)));
}

} // namespace checksum

#endif // CHECKSUM_XXHASH_HPP
