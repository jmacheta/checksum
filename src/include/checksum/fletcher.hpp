#ifndef CHECKSUM_FLETCHER_HPP
#define CHECKSUM_FLETCHER_HPP

#include <checksum/byte_range.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_fletcher Fletcher checksums
 *   Fletcher-16, Fletcher-32 and Fletcher-64, at compile time or at run time.
 *
 *   Fletcher-N reads the message as blocks of N/2 bits: bytes for Fletcher-16, little-endian 16-bit and 32-bit words for
 *   Fletcher-32 and Fletcher-64, with the last block padded with zero bytes. Starting from 0, sum1 adds each block and sum2
 *   adds sum1 after each block, both modulo M = 2^(N/2) - 1; the checksum is (sum2 << N/2) | sum1.
 *   @{
 */

namespace checksum {

/// Running Fletcher-Width checksum. Every value is valid; the default is the empty message.
template <unsigned Width>
  requires(Width == 16 || Width == 32 || Width == 64)
struct fletcher_state {
  /// The checksum: std::uint16_t, std::uint32_t or std::uint64_t.
  using value_type = std::conditional_t<Width == 16, std::uint16_t, std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>>;
  /// One sum: Width/2 bits.
  using sum_type = std::conditional_t<Width == 16, std::uint8_t, std::conditional_t<Width == 32, std::uint16_t, std::uint32_t>>;

  sum_type sum1 = 0;             ///< Sum of the blocks modulo M, including the bytes of an unfinished block; M counts as 0.
  sum_type sum2 = 0;             ///< Sum of sum1 after each finished block, modulo M; M counts as 0.
  std::uint8_t block_offset = 0; ///< Bytes of the unfinished block already in sum1, taken modulo Width/16.

  friend constexpr bool operator==(fletcher_state, fletcher_state) noexcept = default;
};

/// Folds data into state; data may be split anywhere, including inside a block.
template <unsigned Width>
[[nodiscard]] constexpr fletcher_state<Width> fletcher_update(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <unsigned Width, byte_range Range>
[[nodiscard]] constexpr fletcher_state<Width> fletcher_update(fletcher_state<Width> state, Range &&data) noexcept;

/// The checksum of state, an unfinished block padded with zero bytes.
template <unsigned Width> [[nodiscard]] constexpr fletcher_state<Width>::value_type fletcher_finalize(fletcher_state<Width> state) noexcept;

/// fletcher_finalize(fletcher_update(fletcher_state<Width>{}, data)).
template <unsigned Width> [[nodiscard]] constexpr fletcher_state<Width>::value_type fletcher_compute(std::span<std::byte const> data) noexcept;

/// fletcher_finalize(fletcher_update(fletcher_state<Width>{}, data)) for a byte range.
template <unsigned Width, byte_range Range> [[nodiscard]] constexpr fletcher_state<Width>::value_type fletcher_compute(Range &&data) noexcept;

/// Running Fletcher-16 checksum.
using fletcher16_state = fletcher_state<16>;

/// Running Fletcher-32 checksum.
using fletcher32_state = fletcher_state<32>;

/// Running Fletcher-64 checksum.
using fletcher64_state = fletcher_state<64>;

/// fletcher_compute<16>(data).
template <byte_range Range> [[nodiscard]] constexpr std::uint16_t fletcher16_compute(Range &&data) noexcept;

/// fletcher_compute<32>(data).
template <byte_range Range> [[nodiscard]] constexpr std::uint32_t fletcher32_compute(Range &&data) noexcept;

/// fletcher_compute<64>(data).
template <byte_range Range> [[nodiscard]] constexpr std::uint64_t fletcher64_compute(Range &&data) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::fletcher_detail {

// Bytes per block.
template <unsigned Width> inline constexpr unsigned block_size = Width / 16;

// M = 2^(Width/2) - 1.
template <unsigned Width> inline constexpr std::uint64_t modulus = (std::uint64_t{1} << (Width / 2)) - 1;

// Holds the sum of two sums and the checksum: 32 bits where they fit, so that 32-bit targets avoid 64-bit operations.
template <unsigned Width> using integer = std::conditional_t<Width == 64, std::uint64_t, std::uint32_t>;

// value modulo M for value < 2M.
template <unsigned Width> constexpr integer<Width> reduce_once(integer<Width> value) noexcept;

// Folds data byte by byte; the constant-evaluation loop, also used at run time for the bytes of an unfinished block.
template <unsigned Width> constexpr fletcher_state<Width> update_bytes(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// The same fold at run time, defined in src/fletcher/sum_loop.cpp.
fletcher_state<16> sum_loop(fletcher_state<16> state, std::span<std::byte const> data) noexcept;
fletcher_state<32> sum_loop(fletcher_state<32> state, std::span<std::byte const> data) noexcept;
fletcher_state<64> sum_loop(fletcher_state<64> state, std::span<std::byte const> data) noexcept;

} // namespace checksum::fletcher_detail

///@}
///@}

namespace checksum::fletcher_detail {

template <unsigned Width> constexpr integer<Width> reduce_once(integer<Width> value) noexcept {
  return value >= modulus<Width> ? static_cast<integer<Width>>(value - modulus<Width>) : value;
}

template <unsigned Width> constexpr fletcher_state<Width> update_bytes(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  integer<Width> sum1 = reduce_once<Width>(state.sum1);
  integer<Width> sum2 = reduce_once<Width>(state.sum2);
  unsigned offset = state.block_offset % block_size<Width>;
  for(std::byte const byte : data) {
    sum1 = reduce_once<Width>(sum1 + (std::to_integer<integer<Width>>(byte) << (8 * offset)));
    if(++offset == block_size<Width>) {
      offset = 0;
      sum2 = reduce_once<Width>(sum2 + sum1);
    }
  }
  return {.sum1 = static_cast<sum_type>(sum1), .sum2 = static_cast<sum_type>(sum2), .block_offset = static_cast<std::uint8_t>(offset)};
}

} // namespace checksum::fletcher_detail

namespace checksum {

template <unsigned Width> constexpr fletcher_state<Width> fletcher_update(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  if consteval {
    return fletcher_detail::update_bytes(state, data);
  } else {
    return fletcher_detail::sum_loop(state, data);
  }
}

template <unsigned Width, byte_range Range> constexpr fletcher_state<Width> fletcher_update(fletcher_state<Width> state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = fletcher_update(state, chunk); });
  return state;
}

template <unsigned Width> constexpr fletcher_state<Width>::value_type fletcher_finalize(fletcher_state<Width> state) noexcept {
  using value_type = fletcher_state<Width>::value_type;
  fletcher_detail::integer<Width> const sum1 = fletcher_detail::reduce_once<Width>(state.sum1);
  fletcher_detail::integer<Width> sum2 = fletcher_detail::reduce_once<Width>(state.sum2);
  // The zero padding adds nothing to sum1, but the padded block still adds sum1 to sum2.
  if(state.block_offset % fletcher_detail::block_size<Width> != 0) {
    sum2 = fletcher_detail::reduce_once<Width>(sum2 + sum1);
  }
  return static_cast<value_type>((sum2 << (Width / 2)) | sum1);
}

template <unsigned Width> constexpr fletcher_state<Width>::value_type fletcher_compute(std::span<std::byte const> data) noexcept {
  return fletcher_finalize(fletcher_update(fletcher_state<Width>{}, data));
}

template <unsigned Width, byte_range Range> constexpr fletcher_state<Width>::value_type fletcher_compute(Range &&data) noexcept {
  return fletcher_finalize(fletcher_update(fletcher_state<Width>{}, std::forward<Range>(data)));
}

template <byte_range Range> constexpr std::uint16_t fletcher16_compute(Range &&data) noexcept {
  return fletcher_compute<16>(std::forward<Range>(data));
}

template <byte_range Range> constexpr std::uint32_t fletcher32_compute(Range &&data) noexcept {
  return fletcher_compute<32>(std::forward<Range>(data));
}

template <byte_range Range> constexpr std::uint64_t fletcher64_compute(Range &&data) noexcept {
  return fletcher_compute<64>(std::forward<Range>(data));
}

} // namespace checksum

#endif // CHECKSUM_FLETCHER_HPP
