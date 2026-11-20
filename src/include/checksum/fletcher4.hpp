#ifndef CHECKSUM_FLETCHER4_HPP
#define CHECKSUM_FLETCHER4_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_fletcher4 ZFS fletcher4 checksum
 *   The fletcher4 checksum of OpenZFS, at compile time or at run time.
 *
 *   The message is read as little-endian 32-bit words on every host, which is what ZFS computes on little-endian machines.
 *   Starting from 0, for each word sum1 adds the word, sum2 adds sum1, sum3 adds sum2 and sum4 adds sum3, all modulo 2^64;
 *   the checksum is {sum1, sum2, sum3, sum4}. ZFS requires whole words; this library extends it by padding the last word with
 *   zero bytes.
 *   @{
 */

namespace checksum {

/// The checksum: {sum1, sum2, sum3, sum4}, the order of the words of ZFS's zio_cksum_t.
using fletcher4_value = std::array<std::uint64_t, 4>;

/// Running fletcher4 checksum. Every value is valid; the default is the empty message.
struct fletcher4_state {
  std::uint64_t sum1 = 0;       ///< Sum of the words, including the bytes of an unfinished word.
  std::uint64_t sum2 = 0;       ///< Sum of sum1 after each finished word.
  std::uint64_t sum3 = 0;       ///< Sum of sum2 after each finished word.
  std::uint64_t sum4 = 0;       ///< Sum of sum3 after each finished word.
  std::uint8_t word_offset = 0; ///< Bytes of the unfinished word already in sum1, taken modulo 4.

  friend constexpr bool operator==(fletcher4_state, fletcher4_state) noexcept = default;
};

/// fletcher4 with the run-time loop compiled for the instruction-set preset Isa (checksum/isa.hpp). Empty: make one wherever it
/// is needed.
template <isa Isa = isa::generic>
  requires(isa_available(Isa))
class fletcher4_engine {
public:
  using state_type = fletcher4_state; ///< Running checksum.
  using value_type = fletcher4_value; ///< The checksum.

  /// The preset whose loop the engine runs: Isa.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere, including inside a word.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The checksum of state, an unfinished word padded with zero bytes.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// State of a message whose checksum is checksum. That message must end on a whole word, since a checksum does not record an
  /// unfinished word.
  [[nodiscard]] constexpr state_type resume(value_type checksum) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// fletcher4_engine<Isa>{}.compute(data).
template <isa Isa = isa::generic, byte_range Range>
[[nodiscard]] constexpr fletcher4_engine<Isa>::value_type fletcher4_compute(Range &&data) noexcept;

/// A fletcher4_engine whose preset is chosen when it is made, by default the best one of this build that the running CPU executes.
class fletcher4_dispatch_engine : public detail::dispatch_engine<fletcher4_engine<>, isa::generic> {
public:
  /// The best preset of this build in cpu: by default the presets the running CPU executes, or an explicit choice.
  constexpr explicit fletcher4_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::fletcher4_detail {

// Bytes per word.
inline constexpr unsigned word_size = 4;

// Interleaved lanes: lane j folds the words j, j + 4, j + 8, ... as a message of its own.
inline constexpr std::size_t lane_count = 4;

// lane_sums[source][lane], starting at 0: the sum1 to sum4 of each lane, so that one sum of all lanes is contiguous.
using lane_sums = std::array<std::array<std::uint64_t, lane_count>, 4>;

// Adds sum1 to sum2, sum2 to sum3 and sum3 to sum4, as at the end of a word.
constexpr void finish_word(fletcher4_state &state) noexcept;

// The checksum of state, an unfinished word padded with zero bytes.
constexpr fletcher4_value finish(fletcher4_state state) noexcept;

// Folds data byte by byte; the constant-evaluation loop, also used to finish an unfinished word at run time.
constexpr fletcher4_state update_bytes(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The same fold at run time for a state at a word boundary, compiled for the preset Isa: src/fletcher4/ defines it for every
// preset isa_available() accepts.
template <isa Isa> fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept;

// fletcher4_compute() at run time, compiled for the preset Isa: src/fletcher4/ defines it for every preset isa_available() accepts.
// Built there, the empty state stays in registers; GCC zeroes one built by the caller with memset calls.
template <isa Isa> fletcher4_value compute_loop(std::span<std::byte const> data) noexcept;

} // namespace checksum::fletcher4_detail

///@}
///@}

namespace checksum::fletcher4_detail {

constexpr void finish_word(fletcher4_state &state) noexcept {
  state.sum2 += state.sum1;
  state.sum3 += state.sum2;
  state.sum4 += state.sum3;
}

constexpr fletcher4_value finish(fletcher4_state state) noexcept {
  // The zero padding adds nothing to sum1, but the padded word still adds to sum2, sum3 and sum4.
  if(state.word_offset % word_size != 0) {
    finish_word(state);
  }
  return {state.sum1, state.sum2, state.sum3, state.sum4};
}

constexpr fletcher4_state update_bytes(fletcher4_state state, std::span<std::byte const> data) noexcept {
  unsigned offset = state.word_offset % word_size;
  for(std::byte const byte : data) {
    state.sum1 += std::to_integer<std::uint64_t>(byte) << (8 * offset);
    if(++offset == word_size) {
      offset = 0;
      finish_word(state);
    }
  }
  state.word_offset = static_cast<std::uint8_t>(offset);
  return state;
}

} // namespace checksum::fletcher4_detail

namespace checksum {

template <isa Isa>
  requires(isa_available(Isa))
constexpr isa fletcher4_engine<Isa>::preset() noexcept {
  return Isa;
}

template <isa Isa>
  requires(isa_available(Isa))
constexpr fletcher4_state fletcher4_engine<Isa>::initial() const noexcept {
  return {};
}

template <isa Isa>
  requires(isa_available(Isa))
constexpr fletcher4_state fletcher4_engine<Isa>::update(fletcher4_state state, std::span<std::byte const> data) const noexcept {
  if consteval {
    return fletcher4_detail::update_bytes(state, data);
  } else {
    constexpr unsigned word_size = fletcher4_detail::word_size;
    // Bytes that finish an unfinished word; the loop starts at a word boundary.
    std::size_t const head = std::min<std::size_t>(data.size(), (word_size - (state.word_offset % word_size)) % word_size);
    state = fletcher4_detail::update_bytes(state, data.first(head));
    if(head == data.size()) {
      return state;
    }
    return fletcher4_detail::sum_loop<Isa>(state, data.subspan(head));
  }
}

template <isa Isa>
  requires(isa_available(Isa))
template <byte_range Range>
constexpr fletcher4_state fletcher4_engine<Isa>::update(fletcher4_state state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <isa Isa>
  requires(isa_available(Isa))
constexpr fletcher4_value fletcher4_engine<Isa>::finalize(fletcher4_state const &state) const noexcept {
  return fletcher4_detail::finish(state);
}

template <isa Isa>
  requires(isa_available(Isa))
constexpr fletcher4_state fletcher4_engine<Isa>::resume(fletcher4_value checksum) const noexcept {
  return {.sum1 = checksum[0], .sum2 = checksum[1], .sum3 = checksum[2], .sum4 = checksum[3]};
}

template <isa Isa>
  requires(isa_available(Isa))
constexpr fletcher4_value fletcher4_engine<Isa>::compute(std::span<std::byte const> data) const noexcept {
  if consteval {
    return finalize(update(initial(), data));
  } else {
    return fletcher4_detail::compute_loop<Isa>(data);
  }
}

template <isa Isa>
  requires(isa_available(Isa))
template <byte_range Range>
constexpr fletcher4_value fletcher4_engine<Isa>::compute(Range &&data) const noexcept {
  // Contiguous ranges take compute_loop() too.
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

template <isa Isa, byte_range Range> constexpr fletcher4_engine<Isa>::value_type fletcher4_compute(Range &&data) noexcept {
  return fletcher4_engine<Isa>{}.compute(std::forward<Range>(data));
}

constexpr fletcher4_dispatch_engine::fletcher4_dispatch_engine(isa_set cpu) noexcept : dispatch_engine(cpu) {}

} // namespace checksum

#endif // CHECKSUM_FLETCHER4_HPP
