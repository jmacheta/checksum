#ifndef CHECKSUM_ADLER32_HPP
#define CHECKSUM_ADLER32_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

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

/// Adler-32 with the run-time loop compiled for the instruction-set preset Isa (checksum/isa.hpp). Empty: make one wherever it
/// is needed.
template <isa Isa = isa::generic> class adler32_engine {
  static_assert(isa_available(Isa),
                "This build has no variant for that instruction-set preset: isa_available() rejects it on this architecture and compiler.");

public:
  using state_type = adler32_state; ///< Running checksum.
  using value_type = std::uint32_t; ///< The checksum.

  /// The preset whose loop the engine runs: Isa.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The checksum of state.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// State of a message whose checksum is checksum, like the adler argument of zlib's adler32(adler, buf, len).
  [[nodiscard]] constexpr state_type resume(value_type checksum) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// adler32_engine<Isa>{}.compute(data).
template <isa Isa = isa::generic, byte_range Range> [[nodiscard]] constexpr adler32_engine<Isa>::value_type adler32_compute(Range &&data) noexcept;

/// An adler32_engine whose preset is chosen when it is made, by default the best one of this build that the running CPU executes.
class adler32_dispatch_engine : public detail::dispatch_engine<adler32_engine<>> {
public:
  /// The best preset of this build in cpu: by default the presets the running CPU executes, or an explicit choice.
  constexpr explicit adler32_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::adler32_detail {

// The largest prime below 2^16.
inline constexpr std::uint32_t modulus = 65'521;

// value modulo 65521 for value < 2 * 65521.
constexpr std::uint32_t reduce_once(std::uint32_t value) noexcept;

// Folds data byte by byte; the constant-evaluation loop.
constexpr adler32_state update_bytes(adler32_state state, std::span<std::byte const> data) noexcept;

// The same fold at run time, compiled for the preset Isa: src/adler32/ defines it for every preset isa_available() accepts.
template <isa Isa> adler32_state sum_loop(adler32_state state, std::span<std::byte const> data) noexcept;

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

template <isa Isa> constexpr isa adler32_engine<Isa>::preset() noexcept { return Isa; }

template <isa Isa> constexpr adler32_state adler32_engine<Isa>::initial() const noexcept { return {}; }

template <isa Isa> constexpr adler32_state adler32_engine<Isa>::update(adler32_state state, std::span<std::byte const> data) const noexcept {
  if consteval {
    return adler32_detail::update_bytes(state, data);
  } else {
    return adler32_detail::sum_loop<Isa>(state, data);
  }
}

template <isa Isa> template <byte_range Range> constexpr adler32_state adler32_engine<Isa>::update(adler32_state state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <isa Isa> constexpr std::uint32_t adler32_engine<Isa>::finalize(adler32_state const &state) const noexcept {
  return (adler32_detail::reduce_once(state.sum2) << 16U) | adler32_detail::reduce_once(state.sum1);
}

template <isa Isa> constexpr adler32_state adler32_engine<Isa>::resume(std::uint32_t checksum) const noexcept {
  return {.sum1 = static_cast<std::uint16_t>(checksum), .sum2 = static_cast<std::uint16_t>(checksum >> 16U)};
}

template <isa Isa> constexpr std::uint32_t adler32_engine<Isa>::compute(std::span<std::byte const> data) const noexcept {
  return finalize(update(initial(), data));
}

template <isa Isa> template <byte_range Range> constexpr std::uint32_t adler32_engine<Isa>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <isa Isa, byte_range Range> constexpr adler32_engine<Isa>::value_type adler32_compute(Range &&data) noexcept {
  return adler32_engine<Isa>{}.compute(std::forward<Range>(data));
}

constexpr adler32_dispatch_engine::adler32_dispatch_engine(isa_set cpu) noexcept : dispatch_engine(cpu) {}

} // namespace checksum

#endif // CHECKSUM_ADLER32_HPP
