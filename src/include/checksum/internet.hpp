#ifndef CHECKSUM_INTERNET_HPP
#define CHECKSUM_INTERNET_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
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

/// The Internet checksum with the run-time loop compiled for the instruction-set preset Isa (checksum/isa.hpp). Empty: make one
/// wherever it is needed.
template <isa Isa = isa::generic> class internet_engine {
  static_assert(isa_available(Isa),
                "This build has no variant for that instruction-set preset: isa_available() rejects it on this architecture and compiler.");

public:
  using state_type = internet_state; ///< Running checksum.
  using value_type = std::uint16_t;  ///< The checksum.

  /// The preset whose loop the engine runs: Isa.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere, including inside a 16-bit word.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The checksum of state: the one's complement of the sum, a last odd byte padded with zero. Store it most significant byte first.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// State of a message whose checksum is checksum. That message must have an even number of bytes, since a checksum does not record
  /// an odd last byte.
  [[nodiscard]] constexpr state_type resume(value_type checksum) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// internet_engine<Isa>{}.compute(data).
template <isa Isa = isa::generic, byte_range Range> [[nodiscard]] constexpr internet_engine<Isa>::value_type internet_compute(Range &&data) noexcept;

/// An internet_engine whose preset is chosen when it is made, by default the best one of this build that the running CPU executes.
class internet_dispatch_engine : public detail::dispatch_engine<internet_engine<>> {
public:
  /// The best preset of this build in cpu: by default the presets the running CPU executes, or an explicit choice.
  constexpr explicit internet_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::internet_detail {

// Folds sum to 16 bits with end-around carries, keeping it modulo 0xFFFF; the result is 0 only if sum is 0.
template <std::unsigned_integral Value> constexpr std::uint16_t fold(Value sum) noexcept;

// Sum of data as 16-bit big-endian words, a last odd byte padded with zero; the constant-evaluation loop.
constexpr std::uint16_t sum_bytes(std::span<std::byte const> data) noexcept;

// The same sum at run time, compiled for the preset Isa: src/internet/ defines it for every preset isa_available() accepts.
template <isa Isa> std::uint16_t sum_loop(std::span<std::byte const> data) noexcept;

// Folds a chunk's sum, from sum_bytes() or any loop, into state; size is the chunk's length.
constexpr internet_state combine(internet_state state, std::uint16_t sum, std::size_t size) noexcept;

} // namespace checksum::internet_detail

///@}
///@}

namespace checksum::internet_detail {

template <std::unsigned_integral Value> constexpr std::uint16_t fold(Value sum) noexcept {
  static_assert(std::numeric_limits<Value>::digits <= 64);
  // The upper half of sum plus sum rotated by half its width is the end-around-carry sum of both halves.
  std::uint32_t half = 0;
  if constexpr(std::numeric_limits<Value>::digits > 32) {
    std::uint64_t const wide = sum;
    half = static_cast<std::uint32_t>((wide + std::rotr(wide, 32)) >> 32U);
  } else {
    half = sum;
  }
  return static_cast<std::uint16_t>((half + std::rotr(half, 16)) >> 16U);
}

constexpr std::uint16_t sum_bytes(std::span<std::byte const> data) noexcept {
  std::uint64_t sum = 0;
  for(std::size_t index = 0; index < data.size(); ++index) {
    sum += std::to_integer<std::uint64_t>(data[index]) << (index % 2 == 0 ? 8U : 0U);
  }
  return fold(sum);
}

constexpr internet_state combine(internet_state state, std::uint16_t sum, std::size_t size) noexcept {
  // Swapping the bytes of a one's complement sum shifts the message by one byte.
  if(state.odd) {
    sum = std::byteswap(sum);
  }
  return {.sum = fold(std::uint32_t{state.sum} + sum), .odd = state.odd != (size % 2 == 1)};
}

} // namespace checksum::internet_detail

namespace checksum {

template <isa Isa> constexpr isa internet_engine<Isa>::preset() noexcept { return Isa; }

template <isa Isa> constexpr internet_state internet_engine<Isa>::initial() const noexcept { return {}; }

template <isa Isa> constexpr internet_state internet_engine<Isa>::update(internet_state state, std::span<std::byte const> data) const noexcept {
  if consteval {
    return internet_detail::combine(state, internet_detail::sum_bytes(data), data.size());
  } else {
    return internet_detail::combine(state, internet_detail::sum_loop<Isa>(data), data.size());
  }
}

template <isa Isa>
template <byte_range Range>
constexpr internet_state internet_engine<Isa>::update(internet_state state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <isa Isa> constexpr std::uint16_t internet_engine<Isa>::finalize(internet_state const &state) const noexcept {
  return static_cast<std::uint16_t>(~state.sum);
}

template <isa Isa> constexpr internet_state internet_engine<Isa>::resume(std::uint16_t checksum) const noexcept {
  return {.sum = static_cast<std::uint16_t>(~checksum)};
}

template <isa Isa> constexpr std::uint16_t internet_engine<Isa>::compute(std::span<std::byte const> data) const noexcept {
  return finalize(update(initial(), data));
}

template <isa Isa> template <byte_range Range> constexpr std::uint16_t internet_engine<Isa>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <isa Isa, byte_range Range> constexpr internet_engine<Isa>::value_type internet_compute(Range &&data) noexcept {
  return internet_engine<Isa>{}.compute(std::forward<Range>(data));
}

constexpr internet_dispatch_engine::internet_dispatch_engine(isa_set cpu) noexcept : dispatch_engine(cpu) {}

} // namespace checksum

#endif // CHECKSUM_INTERNET_HPP
