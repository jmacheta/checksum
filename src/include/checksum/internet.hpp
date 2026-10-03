#ifndef CHECKSUM_INTERNET_HPP
#define CHECKSUM_INTERNET_HPP

#include <checksum/byte_range.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_internet Internet checksum
 *   The 16-bit one's complement checksum of IPv4, ICMP, UDP and TCP (RFC 1071), at compile time or at run time.
 *   @{
 */

namespace checksum {

/// Running Internet checksum: the one's complement sum of the message as 16-bit big-endian words. Every value is valid; the default
/// is the empty message.
struct internet_state {
  std::uint16_t sum = 0; ///< One's complement sum; 0 only if every byte folded so far is 0.
  bool odd = false;      ///< An odd number of bytes was folded, so the next byte is the low byte of a word.

  friend constexpr bool operator==(internet_state, internet_state) noexcept = default;
};

/// Folds data into state; data may be split anywhere, including inside a 16-bit word.
[[nodiscard]] constexpr internet_state internet_update(internet_state state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <byte_range Range> [[nodiscard]] constexpr internet_state internet_update(internet_state state, Range &&data) noexcept;

/// The checksum of state: the one's complement of the sum, a last odd byte padded with zero. Store it most significant byte first.
[[nodiscard]] constexpr std::uint16_t internet_finalize(internet_state state) noexcept;

/// internet_finalize(internet_update({}, data)).
[[nodiscard]] constexpr std::uint16_t internet_compute(std::span<std::byte const> data) noexcept;

/// internet_finalize(internet_update({}, data)) for a byte range.
template <byte_range Range> [[nodiscard]] constexpr std::uint16_t internet_compute(Range &&data) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::internet_detail {

// Folds sum to 16 bits with end-around carries, keeping it modulo 0xFFFF; the result is 0 only if sum is 0.
constexpr std::uint16_t fold(std::uint64_t sum) noexcept;

// Sum of data as 16-bit big-endian words, a last odd byte padded with zero; the constant-evaluation loop.
constexpr std::uint16_t sum_bytes(std::span<std::byte const> data) noexcept;

// The same sum at run time, defined in src/internet/sum_loop.cpp.
std::uint16_t sum_loop(std::span<std::byte const> data) noexcept;

} // namespace checksum::internet_detail

///@}
///@}

namespace checksum::internet_detail {

constexpr std::uint16_t fold(std::uint64_t sum) noexcept {
  sum = (sum & 0xFFFFFFFFU) + (sum >> 32U); // < 2^33
  sum = (sum & 0xFFFFU) + (sum >> 16U);     // < 2^17 + 2^16
  sum = (sum & 0xFFFFU) + (sum >> 16U);     // <= 0x10001
  sum = (sum & 0xFFFFU) + (sum >> 16U);
  return static_cast<std::uint16_t>(sum);
}

constexpr std::uint16_t sum_bytes(std::span<std::byte const> data) noexcept {
  std::uint64_t sum = 0;
  for(std::size_t index = 0; index < data.size(); ++index) {
    sum += std::to_integer<std::uint64_t>(data[index]) << (index % 2 == 0 ? 8U : 0U);
  }
  return fold(sum);
}

} // namespace checksum::internet_detail

namespace checksum {

constexpr internet_state internet_update(internet_state state, std::span<std::byte const> data) noexcept {
  std::uint16_t sum = 0;
  if consteval {
    sum = internet_detail::sum_bytes(data);
  } else {
    sum = internet_detail::sum_loop(data);
  }
  // Swapping the bytes of a one's complement sum shifts the message by one byte.
  if(state.odd) {
    sum = std::byteswap(sum);
  }
  return {.sum = internet_detail::fold(std::uint64_t{state.sum} + sum), .odd = state.odd != (data.size() % 2 == 1)};
}

template <byte_range Range> constexpr internet_state internet_update(internet_state state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = internet_update(state, chunk); });
  return state;
}

constexpr std::uint16_t internet_finalize(internet_state state) noexcept { return static_cast<std::uint16_t>(~state.sum); }

constexpr std::uint16_t internet_compute(std::span<std::byte const> data) noexcept { return internet_finalize(internet_update({}, data)); }

template <byte_range Range> constexpr std::uint16_t internet_compute(Range &&data) noexcept {
  return internet_finalize(internet_update({}, std::forward<Range>(data)));
}

} // namespace checksum

#endif // CHECKSUM_INTERNET_HPP
