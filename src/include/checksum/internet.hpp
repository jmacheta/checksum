#ifndef CHECKSUM_INTERNET_HPP
#define CHECKSUM_INTERNET_HPP

#include <checksum/byte_range.hpp>
#include <checksum/isa.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
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

/// Strategy of the run-time loop: the portable loop, or the CPU kernel that the build's compiler flags enable. Every
/// call without a strategy uses it.
struct internet_loop {};

/// Strategy of the run-time loop compiled for the instruction set Isa, whatever the build's compiler flags:
/// internet_compute<internet_loop_for<isa_x86_avx2>>(data). A build compiles such a loop only on architectures that
/// support it; asking for one it did not compile is a compile error, not a link error.
template <isa Isa> struct internet_loop_for {};

/// Implementation details; not part of the public API.
namespace internet_detail {

// internet_loop_for<Isa> and its instruction set, or nothing for any other type.
template <class Strategy> struct loop_isa;
template <isa Isa> struct loop_isa<internet_loop_for<Isa>> {
  using type = Isa;
};

} // namespace internet_detail

/// The strategies internet_update() and internet_compute() accept.
template <class Strategy>
concept internet_strategy = std::same_as<Strategy, internet_loop> || requires { typename internet_detail::loop_isa<Strategy>::type; };

/// Folds data into state; data may be split anywhere, including inside a 16-bit word. Strategy selects the run-time loop.
template <internet_strategy Strategy = internet_loop>
[[nodiscard]] constexpr internet_state internet_update(internet_state state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <internet_strategy Strategy = internet_loop, byte_range Range>
[[nodiscard]] constexpr internet_state internet_update(internet_state state, Range &&data) noexcept;

/// Folds data into the message whose checksum is checksum and returns the new checksum. That message must have an even number of
/// bytes, since a checksum does not record an odd last byte.
template <internet_strategy Strategy = internet_loop, std::same_as<std::uint16_t> Checksum>
[[nodiscard]] constexpr std::uint16_t internet_update(Checksum checksum, std::span<std::byte const> data) noexcept;

/// Folds a byte range into the message whose checksum is checksum and returns the new checksum; that message has an even length.
template <internet_strategy Strategy = internet_loop, std::same_as<std::uint16_t> Checksum, byte_range Range>
[[nodiscard]] constexpr std::uint16_t internet_update(Checksum checksum, Range &&data) noexcept;

/// The checksum of state: the one's complement of the sum, a last odd byte padded with zero. Store it most significant byte first.
[[nodiscard]] constexpr std::uint16_t internet_finalize(internet_state state) noexcept;

/// internet_finalize(internet_update(internet_state{}, data)).
template <internet_strategy Strategy = internet_loop>
[[nodiscard]] constexpr std::uint16_t internet_compute(std::span<std::byte const> data) noexcept;

/// internet_finalize(internet_update(internet_state{}, data)) for a byte range.
template <internet_strategy Strategy = internet_loop, byte_range Range> [[nodiscard]] constexpr std::uint16_t internet_compute(Range &&data) noexcept;

/// Folds data into state with the run-time loop that isa selects: none runs the loop of internet_loop, x86_avx2 the loop
/// of internet_loop_for<isa_x86_avx2>. A variant this build did not compile, or the running CPU does not support, runs
/// the loop of internet_loop instead; detect_isa() returns the best one available. Not constexpr.
[[nodiscard]] internet_state internet_update(internet_state state, std::span<std::byte const> data, isa_id isa) noexcept;

/// Folds a byte range into state with the run-time loop that isa selects.
template <byte_range Range> [[nodiscard]] internet_state internet_update(internet_state state, Range &&data, isa_id isa) noexcept;

/// Folds data into the message whose checksum is checksum and returns the new checksum, with the run-time loop that isa
/// selects. That message must have an even number of bytes.
template <std::same_as<std::uint16_t> Checksum>
[[nodiscard]] std::uint16_t internet_update(Checksum checksum, std::span<std::byte const> data, isa_id isa) noexcept;

/// Folds a byte range into the message whose checksum is checksum and returns the new checksum, with the run-time loop
/// that isa selects; that message has an even length.
template <std::same_as<std::uint16_t> Checksum, byte_range Range>
[[nodiscard]] std::uint16_t internet_update(Checksum checksum, Range &&data, isa_id isa) noexcept;

/// internet_finalize(internet_update(internet_state{}, data, isa)).
[[nodiscard]] std::uint16_t internet_compute(std::span<std::byte const> data, isa_id isa) noexcept;

/// internet_finalize(internet_update(internet_state{}, data, isa)) for a byte range.
template <byte_range Range> [[nodiscard]] std::uint16_t internet_compute(Range &&data, isa_id isa) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::internet_detail {

// Folds sum to 16 bits with end-around carries, keeping it modulo 0xFFFF; the result is 0 only if sum is 0.
template <std::unsigned_integral Value> constexpr std::uint16_t fold(Value sum) noexcept;

// Sum of data as 16-bit big-endian words, a last odd byte padded with zero; the constant-evaluation loop.
constexpr std::uint16_t sum_bytes(std::span<std::byte const> data) noexcept;

// The same sum at run time, defined in src/internet/sum_loop.cpp, with the kernel the build's compiler flags enable.
std::uint16_t sum_loop(std::span<std::byte const> data) noexcept;

// The same sum at run time compiled for x86-64 AVX2, defined in src/internet/sum_loop_x86_avx2.cpp only in builds that
// compile that source; such a build defines CHECKSUM_INTERNET_X86_AVX2.
std::uint16_t sum_loop_x86_avx2(std::span<std::byte const> data) noexcept;

// Whether this build compiled the run-time loop of internet_loop_for<Isa>.
template <isa Isa> inline constexpr bool loop_compiled = false;
#ifdef CHECKSUM_INTERNET_X86_AVX2
template <> inline constexpr bool loop_compiled<isa_x86_avx2> = true;
#endif

// The run-time sum of data with the loop of the strategy; never reached by constant evaluation, which runs sum_bytes().
template <internet_strategy Strategy> std::uint16_t strategy_sum(std::span<std::byte const> data) noexcept;

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

template <internet_strategy Strategy> std::uint16_t strategy_sum(std::span<std::byte const> data) noexcept {
  if constexpr(std::same_as<Strategy, internet_loop>) {
    return sum_loop(data);
  } else if constexpr(std::same_as<Strategy, internet_loop_for<isa_x86_avx2>>) {
    // The build compiles that loop only for x86-64 with GCC or Clang, and defines CHECKSUM_INTERNET_X86_AVX2 when it does.
    static_assert(loop_compiled<isa_x86_avx2>, "internet_loop_for<isa_x86_avx2> was not compiled into this build; use internet_loop instead.");
    return sum_loop_x86_avx2(data);
  } else {
    // Every instruction set with a compiled loop gets a branch above; keep the two in step.
    static_assert(loop_compiled<typename loop_isa<Strategy>::type>,
                  "internet_loop_for<Isa> has no loop for that instruction set; use internet_loop instead.");
    return sum_loop(data);
  }
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

template <internet_strategy Strategy> constexpr internet_state internet_update(internet_state state, std::span<std::byte const> data) noexcept {
  if consteval {
    return internet_detail::combine(state, internet_detail::sum_bytes(data), data.size());
  }
  return internet_detail::combine(state, internet_detail::strategy_sum<Strategy>(data), data.size());
}

template <internet_strategy Strategy, byte_range Range> constexpr internet_state internet_update(internet_state state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = internet_update<Strategy>(state, chunk); });
  return state;
}

constexpr std::uint16_t internet_finalize(internet_state state) noexcept { return static_cast<std::uint16_t>(~state.sum); }

template <internet_strategy Strategy, std::same_as<std::uint16_t> Checksum>
constexpr std::uint16_t internet_update(Checksum checksum, std::span<std::byte const> data) noexcept {
  return internet_finalize(internet_update<Strategy>(internet_state{.sum = static_cast<std::uint16_t>(~checksum)}, data));
}

template <internet_strategy Strategy, std::same_as<std::uint16_t> Checksum, byte_range Range>
constexpr std::uint16_t internet_update(Checksum checksum, Range &&data) noexcept {
  return internet_finalize(internet_update<Strategy>(internet_state{.sum = static_cast<std::uint16_t>(~checksum)}, std::forward<Range>(data)));
}

template <internet_strategy Strategy> constexpr std::uint16_t internet_compute(std::span<std::byte const> data) noexcept {
  return internet_finalize(internet_update<Strategy>(internet_state{}, data));
}

template <internet_strategy Strategy, byte_range Range> constexpr std::uint16_t internet_compute(Range &&data) noexcept {
  return internet_finalize(internet_update<Strategy>(internet_state{}, std::forward<Range>(data)));
}

template <byte_range Range> internet_state internet_update(internet_state state, Range &&data, isa_id isa) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = internet_update(state, chunk, isa); });
  return state;
}

template <std::same_as<std::uint16_t> Checksum>
std::uint16_t internet_update(Checksum checksum, std::span<std::byte const> data, isa_id isa) noexcept {
  return internet_finalize(internet_update(internet_state{.sum = static_cast<std::uint16_t>(~checksum)}, data, isa));
}

template <std::same_as<std::uint16_t> Checksum, byte_range Range>
std::uint16_t internet_update(Checksum checksum, Range &&data, isa_id isa) noexcept {
  return internet_finalize(internet_update(internet_state{.sum = static_cast<std::uint16_t>(~checksum)}, std::forward<Range>(data), isa));
}

inline std::uint16_t internet_compute(std::span<std::byte const> data, isa_id isa) noexcept {
  return internet_finalize(internet_update(internet_state{}, data, isa));
}

template <byte_range Range> std::uint16_t internet_compute(Range &&data, isa_id isa) noexcept {
  return internet_finalize(internet_update(internet_state{}, std::forward<Range>(data), isa));
}

} // namespace checksum

#endif // CHECKSUM_INTERNET_HPP
