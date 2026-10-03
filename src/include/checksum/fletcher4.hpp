#ifndef CHECKSUM_FLETCHER4_HPP
#define CHECKSUM_FLETCHER4_HPP

#include <checksum/byte_range.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

/// Folds data into state; data may be split anywhere, including inside a word.
[[nodiscard]] constexpr fletcher4_state fletcher4_update(fletcher4_state state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <byte_range Range> [[nodiscard]] constexpr fletcher4_state fletcher4_update(fletcher4_state state, Range &&data) noexcept;

/// The checksum of state, an unfinished word padded with zero bytes.
[[nodiscard]] constexpr fletcher4_value fletcher4_finalize(fletcher4_state state) noexcept;

/// fletcher4_finalize(fletcher4_update(fletcher4_state{}, data)).
[[nodiscard]] constexpr fletcher4_value fletcher4_compute(std::span<std::byte const> data) noexcept;

/// fletcher4_finalize(fletcher4_update(fletcher4_state{}, data)) for a byte range.
template <byte_range Range> [[nodiscard]] constexpr fletcher4_value fletcher4_compute(Range &&data) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::fletcher4_detail {

// Bytes per word.
inline constexpr unsigned word_size = 4;

// Adds sum1 to sum2, sum2 to sum3 and sum3 to sum4, as at the end of a word.
constexpr void finish_word(fletcher4_state &state) noexcept;

// Folds data byte by byte; the constant-evaluation loop, also used to finish an unfinished word at run time.
constexpr fletcher4_state update_bytes(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The same fold at run time for a state at a word boundary, defined in src/fletcher4/sum_loop.cpp.
fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept;

// fletcher4_compute() at run time, defined in src/fletcher4/sum_loop.cpp. Built there, the empty state stays in registers; GCC zeroes
// one built by the caller with memset calls.
fletcher4_value compute_loop(std::span<std::byte const> data) noexcept;

} // namespace checksum::fletcher4_detail

///@}
///@}

namespace checksum::fletcher4_detail {

constexpr void finish_word(fletcher4_state &state) noexcept {
  state.sum2 += state.sum1;
  state.sum3 += state.sum2;
  state.sum4 += state.sum3;
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

constexpr fletcher4_state fletcher4_update(fletcher4_state state, std::span<std::byte const> data) noexcept {
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
    return fletcher4_detail::sum_loop(state, data.subspan(head));
  }
}

template <byte_range Range> constexpr fletcher4_state fletcher4_update(fletcher4_state state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = fletcher4_update(state, chunk); });
  return state;
}

constexpr fletcher4_value fletcher4_finalize(fletcher4_state state) noexcept {
  // The zero padding adds nothing to sum1, but the padded word still adds to sum2, sum3 and sum4.
  if(state.word_offset % fletcher4_detail::word_size != 0) {
    fletcher4_detail::finish_word(state);
  }
  return {state.sum1, state.sum2, state.sum3, state.sum4};
}

constexpr fletcher4_value fletcher4_compute(std::span<std::byte const> data) noexcept {
  if consteval {
    return fletcher4_finalize(fletcher4_update(fletcher4_state{}, data));
  } else {
    return fletcher4_detail::compute_loop(data);
  }
}

template <byte_range Range> constexpr fletcher4_value fletcher4_compute(Range &&data) noexcept {
  return fletcher4_finalize(fletcher4_update(fletcher4_state{}, std::forward<Range>(data)));
}

} // namespace checksum

#endif // CHECKSUM_FLETCHER4_HPP
