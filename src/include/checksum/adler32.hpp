#ifndef CHECKSUM_ADLER32_HPP
#define CHECKSUM_ADLER32_HPP

#include <checksum/byte_range.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_adler32 Adler-32 checksum
 *   Adler-32 of zlib (RFC 1950), at compile time or at run time.
 *
 *   Starting from sum1 = 1 and sum2 = 0, sum1 adds each byte and sum2 adds sum1 after each byte, both modulo 65521; the
 *   checksum is (sum2 << 16) | sum1.
 *   @{
 */

namespace checksum {

/// Running Adler-32 checksum. Every value is valid; the default is the empty message.
struct adler32_state {
  std::uint16_t sum1 = 1; ///< 1 plus the sum of the bytes, modulo 65521; values from 65521 count modulo 65521.
  std::uint16_t sum2 = 0; ///< Sum of sum1 after each byte, modulo 65521; values from 65521 count modulo 65521.

  friend constexpr bool operator==(adler32_state, adler32_state) noexcept = default;
};

/// Folds data into state; data may be split anywhere.
[[nodiscard]] constexpr adler32_state adler32_update(adler32_state state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <byte_range Range> [[nodiscard]] constexpr adler32_state adler32_update(adler32_state state, Range &&data) noexcept;

/// Folds data into the message whose checksum is checksum and returns the new checksum, like zlib's adler32(adler, buf, len).
/// The checksum must be a std::uint32_t, so that {} still means the empty adler32_state.
template <std::same_as<std::uint32_t> Checksum>
[[nodiscard]] constexpr std::uint32_t adler32_update(Checksum checksum, std::span<std::byte const> data) noexcept;

/// Folds a byte range into the message whose checksum is checksum and returns the new checksum.
template <std::same_as<std::uint32_t> Checksum, byte_range Range>
[[nodiscard]] constexpr std::uint32_t adler32_update(Checksum checksum, Range &&data) noexcept;

/// The checksum of state.
[[nodiscard]] constexpr std::uint32_t adler32_finalize(adler32_state state) noexcept;

/// adler32_finalize(adler32_update(adler32_state{}, data)).
[[nodiscard]] constexpr std::uint32_t adler32_compute(std::span<std::byte const> data) noexcept;

/// adler32_finalize(adler32_update(adler32_state{}, data)) for a byte range.
template <byte_range Range> [[nodiscard]] constexpr std::uint32_t adler32_compute(Range &&data) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::adler32_detail {

// The largest prime below 2^16.
inline constexpr std::uint32_t modulus = 65'521;

// value modulo 65521 for value < 2 * 65521.
constexpr std::uint32_t reduce_once(std::uint32_t value) noexcept;

// Folds data byte by byte; the constant-evaluation loop.
constexpr adler32_state update_bytes(adler32_state state, std::span<std::byte const> data) noexcept;

// The same fold at run time, defined in src/adler32/sum_loop.cpp.
adler32_state sum_loop(adler32_state state, std::span<std::byte const> data) noexcept;

} // namespace checksum::adler32_detail

///@}
///@}

namespace checksum::adler32_detail {

constexpr std::uint32_t reduce_once(std::uint32_t value) noexcept { return value >= modulus ? value - modulus : value; }

constexpr adler32_state update_bytes(adler32_state state, std::span<std::byte const> data) noexcept {
  std::uint32_t sum1 = reduce_once(state.sum1);
  std::uint32_t sum2 = reduce_once(state.sum2);
  for(std::byte const byte : data) {
    sum1 = reduce_once(sum1 + std::to_integer<std::uint32_t>(byte));
    sum2 = reduce_once(sum2 + sum1);
  }
  return {.sum1 = static_cast<std::uint16_t>(sum1), .sum2 = static_cast<std::uint16_t>(sum2)};
}

} // namespace checksum::adler32_detail

namespace checksum {

constexpr adler32_state adler32_update(adler32_state state, std::span<std::byte const> data) noexcept {
  if consteval {
    return adler32_detail::update_bytes(state, data);
  } else {
    return adler32_detail::sum_loop(state, data);
  }
}

template <byte_range Range> constexpr adler32_state adler32_update(adler32_state state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = adler32_update(state, chunk); });
  return state;
}

constexpr std::uint32_t adler32_finalize(adler32_state state) noexcept {
  return (adler32_detail::reduce_once(state.sum2) << 16U) | adler32_detail::reduce_once(state.sum1);
}

template <std::same_as<std::uint32_t> Checksum> constexpr std::uint32_t adler32_update(Checksum checksum, std::span<std::byte const> data) noexcept {
  adler32_state const state{.sum1 = static_cast<std::uint16_t>(checksum), .sum2 = static_cast<std::uint16_t>(checksum >> 16U)};
  return adler32_finalize(adler32_update(state, data));
}

template <std::same_as<std::uint32_t> Checksum, byte_range Range> constexpr std::uint32_t adler32_update(Checksum checksum, Range &&data) noexcept {
  adler32_state const state{.sum1 = static_cast<std::uint16_t>(checksum), .sum2 = static_cast<std::uint16_t>(checksum >> 16U)};
  return adler32_finalize(adler32_update(state, std::forward<Range>(data)));
}

constexpr std::uint32_t adler32_compute(std::span<std::byte const> data) noexcept { return adler32_finalize(adler32_update(adler32_state{}, data)); }

template <byte_range Range> constexpr std::uint32_t adler32_compute(Range &&data) noexcept {
  return adler32_finalize(adler32_update(adler32_state{}, std::forward<Range>(data)));
}

} // namespace checksum

#endif // CHECKSUM_ADLER32_HPP
