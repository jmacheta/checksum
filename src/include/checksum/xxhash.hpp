#ifndef CHECKSUM_XXHASH_HPP
#define CHECKSUM_XXHASH_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
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

/// Running XXH32 (Width 32) or XXH64 (Width 64) hash. Every value is valid; the default is the empty message.
template <unsigned Width>
  requires(Width == 32 || Width == 64)
struct xxhash_state {
  /// Lane accumulators: std::uint32_t or std::uint64_t, set from the seed when the first stripe is folded.
  std::array<std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>, 4> lanes{};
  std::uint64_t length = 0;                  ///< Bytes folded so far.
  std::array<std::byte, Width / 2> buffer{}; ///< The unfinished stripe: its first length % buffer.size() bytes.

  /// Equal if they hash alike with every seed: the same length and unfinished stripe, and the same lanes from the first stripe on.
  friend constexpr bool operator==(xxhash_state const &left, xxhash_state const &right) noexcept {
    auto const buffered = static_cast<std::size_t>(left.length % left.buffer.size());
    return left.length == right.length && (left.length < left.buffer.size() || left.lanes == right.lanes) &&
           std::ranges::equal(std::span(left.buffer).first(buffered), std::span(right.buffer).first(buffered));
  }
};

/// XXH32 (Width 32) or XXH64 (Width 64) with a seed. Every instruction-set preset Isa (checksum/isa.hpp) runs the loop of isa::generic.
template <unsigned Width, isa Isa = isa::generic>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
class xxhash_engine {
public:
  using state_type = xxhash_state<Width>;                                           ///< Running hash.
  using value_type = std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>; ///< The hash and the seed.

  /// The engine of the hash with seed.
  constexpr explicit xxhash_engine(value_type seed = 0) noexcept;

  /// generic, whose loop every preset runs.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The hash of the message folded into state. Messages of 4 GiB or more mix in their length modulo 2^32 for Width 32.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;

private:
  value_type hash_seed;
};

/// xxhash_engine<Width, Isa>{seed}.compute(data).
template <unsigned Width, isa Isa = isa::generic, byte_range Range>
[[nodiscard]] constexpr xxhash_engine<Width, Isa>::value_type xxhash_compute(Range &&data,
                                                                             typename xxhash_engine<Width, Isa>::value_type seed = 0) noexcept;

} // namespace checksum

namespace checksum::xxhash_detail {

// The presets a dispatch engine chooses from: generic alone, whose loop every preset runs.
inline constexpr isa_set own_loops = isa_set{};

} // namespace checksum::xxhash_detail

namespace checksum {

/// An xxhash_engine with the dispatch engines' interface. No preset has a loop of its own, so it always chooses generic.
template <unsigned Width>
  requires(Width == 32 || Width == 64)
class xxhash_dispatch_engine
    : public detail::dispatch_engine<xxhash_engine<Width>, xxhash_detail::own_loops, typename xxhash_engine<Width>::value_type> {
public:
  /// The engine of the hash with seed; cpu does not change the loop.
  constexpr explicit xxhash_dispatch_engine(typename xxhash_engine<Width>::value_type seed = 0, isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::xxhash_detail {

template <unsigned Width> using lane_array = decltype(xxhash_state<Width>::lanes);

template <unsigned Width> using word = lane_array<Width>::value_type;

template <unsigned Width> inline constexpr std::size_t stripe_size = std::tuple_size_v<decltype(xxhash_state<Width>::buffer)>;

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
  static constexpr std::array<unsigned, 3> avalanche_shifts{15, 13, 16};
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
  static constexpr std::array<unsigned, 3> avalanche_shifts{33, 29, 32};
};

using detail::load;

// Mixes one input lane into a lane accumulator.
template <unsigned Width> constexpr word<Width> round(word<Width> accumulator, word<Width> lane) noexcept;

template <unsigned Width> constexpr lane_array<Width> initial_lanes(word<Width> seed) noexcept;

// Merges the four lanes into one value. Spelled out: as a loop, GCC reloads the lanes just stored as one vector, which stalls.
template <unsigned Width> constexpr word<Width> converge(lane_array<Width> const &lanes) noexcept;

// Folds the whole stripes of data into lanes. Behind the call to stripe_loop() the lanes may alias data, which keeps compilers from
// packing them into one vector register: slower than scalar code for XXH32.
template <unsigned Width> constexpr void fold_stripes(lane_array<Width> &lanes, std::span<std::byte const> data) noexcept;

// fold_stripes(), then converge() if wanted, else 0; defined once in src/xxhash/stripe_loop.cpp, the loop of every preset. Returning the
// merged value spares the caller reloading the lanes right after their stores.
template <unsigned Width> word<Width> stripe_loop(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept;

// fold_stripes() and converge() if wanted, during constant evaluation, else stripe_loop().
template <unsigned Width> constexpr word<Width> fold_and_converge(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept;

// The final mix of a hash.
template <unsigned Width> constexpr word<Width> avalanche(word<Width> hash) noexcept;

// The hash of a message of length bytes: converged holds its stripes if length reaches a stripe, tail holds the rest.
template <unsigned Width>
constexpr word<Width> finish(word<Width> converged, word<Width> seed, std::uint64_t length, std::span<std::byte const> tail) noexcept;

// The hash of data of at least one stripe, defined with stripe_loop(). Its lanes are locals, which GCC and Clang keep in
// scalar registers: on Cortex-M4 an XXH32 stripe takes 24.5 cycles, against 28.6 through stripe_loop().
template <unsigned Width> word<Width> run_time_hash(std::span<std::byte const> data, word<Width> seed) noexcept;

} // namespace checksum::xxhash_detail

///@}
///@}

namespace checksum::xxhash_detail {

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
  word<Width> const hash = std::rotl(lanes[0], convergence_rotations[0]) + std::rotl(lanes[1], convergence_rotations[1]) +
                           std::rotl(lanes[2], convergence_rotations[2]) + std::rotl(lanes[3], convergence_rotations[3]);
  if constexpr(Width == 64) {
    // Spelled out: GCC keeps the lanes of a loop here in memory.
    auto const merge = [](word<Width> value, word<Width> lane) {
      return ((value ^ round<Width>(0, lane)) * constants::prime_1) + constants::prime_4;
    };
    return merge(merge(merge(merge(hash, lanes[0]), lanes[1]), lanes[2]), lanes[3]);
  } else {
    return hash;
  }
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

template <unsigned Width>
constexpr word<Width> fold_and_converge(lane_array<Width> &lanes, std::span<std::byte const> data, bool converged) noexcept {
  if consteval {
    fold_stripes<Width>(lanes, data);
    return converged ? converge<Width>(lanes) : 0;
  } else {
    return stripe_loop<Width>(lanes, data, converged);
  }
}

template <unsigned Width> constexpr word<Width> avalanche(word<Width> hash) noexcept {
  using constants = algorithm_constants<Width>;
  hash ^= hash >> constants::avalanche_shifts[0];
  hash *= constants::prime_2;
  hash ^= hash >> constants::avalanche_shifts[1];
  hash *= constants::prime_3;
  return hash ^ (hash >> constants::avalanche_shifts[2]);
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
  return avalanche<Width>(hash);
}

} // namespace checksum::xxhash_detail

namespace checksum {

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr xxhash_engine<Width, Isa>::xxhash_engine(value_type seed) noexcept : hash_seed(seed) {}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr isa xxhash_engine<Width, Isa>::preset() noexcept {
  return isa_detail::loop_preset(Isa, xxhash_detail::own_loops);
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr xxhash_state<Width> xxhash_engine<Width, Isa>::initial() const noexcept {
  return {};
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr xxhash_state<Width> xxhash_engine<Width, Isa>::update(xxhash_state<Width> state, std::span<std::byte const> data) const noexcept {
  constexpr std::size_t stripe_size = xxhash_detail::stripe_size<Width>;
  auto const buffered = static_cast<std::size_t>(state.length % stripe_size);
  if(state.length < stripe_size && data.size() >= stripe_size - buffered) {
    state.lanes = xxhash_detail::initial_lanes<Width>(hash_seed);
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
    xxhash_detail::fold_and_converge<Width>(state.lanes, data.first(whole), false);
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
  return state;
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr xxhash_state<Width> xxhash_engine<Width, Isa>::update(xxhash_state<Width> state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr xxhash_engine<Width, Isa>::value_type xxhash_engine<Width, Isa>::finalize(xxhash_state<Width> const &state) const noexcept {
  auto const buffered = static_cast<std::size_t>(state.length % xxhash_detail::stripe_size<Width>);
  // Before the first stripe the lanes are unused, and converging them would cost 8 multiplications for nothing.
  xxhash_detail::word<Width> const converged = state.length >= xxhash_detail::stripe_size<Width> ? xxhash_detail::converge<Width>(state.lanes) : 0;
  return xxhash_detail::finish<Width>(converged, hash_seed, state.length, std::span(state.buffer).first(buffered));
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr xxhash_engine<Width, Isa>::value_type xxhash_engine<Width, Isa>::compute(std::span<std::byte const> data) const noexcept {
  std::size_t const whole = data.size() - (data.size() % xxhash_detail::stripe_size<Width>);
  // On 32-bit targets the four lanes of XXH64 do not fit the registers: as locals they ran 18 % slower on Cortex-M4.
  if constexpr(sizeof(std::size_t) >= sizeof(xxhash_detail::word<Width>)) {
    if !consteval {
      if(whole != 0) {
        return xxhash_detail::run_time_hash<Width>(data, hash_seed);
      }
    }
  }
  xxhash_detail::word<Width> converged = 0;
  if(whole != 0) {
    xxhash_detail::lane_array<Width> lanes = xxhash_detail::initial_lanes<Width>(hash_seed);
    converged = xxhash_detail::fold_and_converge<Width>(lanes, data.first(whole), true);
  }
  return xxhash_detail::finish<Width>(converged, hash_seed, data.size(), data.subspan(whole));
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr xxhash_engine<Width, Isa>::value_type xxhash_engine<Width, Isa>::compute(Range &&data) const noexcept {
  // Contiguous ranges skip the buffer of the state.
  if constexpr(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>) {
    if !consteval {
      return compute(std::as_bytes(std::span(std::ranges::data(data), std::ranges::size(data))));
    } else {
      return finalize(update(initial(), std::forward<Range>(data)));
    }
  } else {
    return finalize(update(initial(), std::forward<Range>(data)));
  }
}

template <unsigned Width, isa Isa, byte_range Range>
constexpr xxhash_engine<Width, Isa>::value_type xxhash_compute(Range &&data, typename xxhash_engine<Width, Isa>::value_type seed) noexcept {
  return xxhash_engine<Width, Isa>{seed}.compute(std::forward<Range>(data));
}

template <unsigned Width>
  requires(Width == 32 || Width == 64)
constexpr xxhash_dispatch_engine<Width>::xxhash_dispatch_engine(typename xxhash_engine<Width>::value_type seed, isa_set cpu) noexcept
    : detail::dispatch_engine<xxhash_engine<Width>, xxhash_detail::own_loops, typename xxhash_engine<Width>::value_type>(cpu, seed) {}

} // namespace checksum

#endif // CHECKSUM_XXHASH_HPP
