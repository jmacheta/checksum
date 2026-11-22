#ifndef CHECKSUM_FLETCHER_HPP
#define CHECKSUM_FLETCHER_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

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
  /// One sum: Width/2 bits.
  using sum_type = std::conditional_t<Width == 16, std::uint8_t, std::conditional_t<Width == 32, std::uint16_t, std::uint32_t>>;

  sum_type sum1 = 0;             ///< Sum of the blocks modulo M, including the bytes of an unfinished block; M counts as 0.
  sum_type sum2 = 0;             ///< Sum of sum1 after each finished block, modulo M; M counts as 0.
  std::uint8_t block_offset = 0; ///< Bytes of the unfinished block already in sum1, taken modulo Width/16.

  friend constexpr bool operator==(fletcher_state, fletcher_state) noexcept = default;
};

/// Fletcher-Width with the run-time loop compiled for the instruction-set preset Isa (checksum/isa.hpp). Empty: make one wherever
/// it is needed.
template <unsigned Width, isa Isa = isa::generic>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
class fletcher_engine {
public:
  using state_type = fletcher_state<Width>; ///< Running checksum.
  /// The checksum: std::uint16_t, std::uint32_t or std::uint64_t.
  using value_type = std::conditional_t<Width == 16, std::uint16_t, std::conditional_t<Width == 32, std::uint32_t, std::uint64_t>>;

  /// The preset whose loop the engine runs: Isa, or the nearest preset it extends that has a loop of its own (isa_detail::loop_preset()).
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere, including inside a block.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The checksum of state, an unfinished block padded with zero bytes.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// State of a message whose checksum is checksum. That message must end on a whole block, since a checksum does not record an
  /// unfinished block.
  [[nodiscard]] constexpr state_type resume(value_type checksum) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// fletcher_engine<Width, Isa>{}.compute(data).
template <unsigned Width, isa Isa = isa::generic, byte_range Range>
[[nodiscard]] constexpr fletcher_engine<Width, Isa>::value_type fletcher_compute(Range &&data) noexcept;

} // namespace checksum

namespace checksum::fletcher_detail {

// The presets a dispatch engine chooses from: generic, those with target options, and riscv_v, the one without them with a kernel.
inline constexpr isa_set own_loops = isa_detail::built_with_target_options() | isa::riscv_v;

} // namespace checksum::fletcher_detail

namespace checksum {

/// A fletcher_engine whose preset is chosen when it is made, by default the best one of this build that the running CPU executes.
template <unsigned Width>
  requires(Width == 16 || Width == 32 || Width == 64)
class fletcher_dispatch_engine : public detail::dispatch_engine<fletcher_engine<Width>, fletcher_detail::own_loops> {
public:
  /// The best preset of this build in cpu: by default the presets the running CPU executes, or an explicit choice.
  constexpr explicit fletcher_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

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

// The state of Fletcher-16 and Fletcher-32 as one integer, sum1, sum2 and block_offset from the low bits in Width/2 bits each,
// so that compilers pass it in a register; Fletcher-64 passes the state itself.
template <unsigned Width>
using state_word = std::conditional_t<Width == 16, std::uint32_t, std::conditional_t<Width == 32, std::uint64_t, fletcher_state<64>>>;

template <unsigned Width> constexpr state_word<Width> to_word(fletcher_state<Width> state) noexcept;
template <unsigned Width> constexpr fletcher_state<Width> from_word(state_word<Width> word) noexcept;

// The same fold at run time, compiled for the preset Isa: src/fletcher/ defines it for every preset isa_available() accepts.
template <isa Isa> state_word<16> sum_loop(state_word<16> state, std::span<std::byte const> data) noexcept;
template <isa Isa> state_word<32> sum_loop(state_word<32> state, std::span<std::byte const> data) noexcept;
template <isa Isa> state_word<64> sum_loop(state_word<64> state, std::span<std::byte const> data) noexcept;

} // namespace checksum::fletcher_detail

///@}
///@}

namespace checksum::fletcher_detail {

template <unsigned Width> constexpr integer<Width> reduce_once(integer<Width> value) noexcept {
  return value >= modulus<Width> ? static_cast<integer<Width>>(value - modulus<Width>) : value;
}

template <unsigned Width> constexpr state_word<Width> to_word(fletcher_state<Width> state) noexcept {
  if constexpr(Width == 64) {
    return state;
  } else {
    constexpr unsigned bits = Width / 2;
    return static_cast<state_word<Width>>(state.sum1 | (state_word<Width>{state.sum2} << bits) |
                                          (state_word<Width>{state.block_offset} << (2 * bits)));
  }
}

template <unsigned Width> constexpr fletcher_state<Width> from_word(state_word<Width> word) noexcept {
  if constexpr(Width == 64) {
    return word;
  } else {
    using sum_type = fletcher_state<Width>::sum_type;
    constexpr unsigned bits = Width / 2;
    return {.sum1 = static_cast<sum_type>(word),
            .sum2 = static_cast<sum_type>(word >> bits),
            .block_offset = static_cast<std::uint8_t>(word >> (2 * bits))};
  }
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

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr isa fletcher_engine<Width, Isa>::preset() noexcept {
  return isa_detail::loop_preset(Isa, fletcher_detail::own_loops);
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fletcher_state<Width> fletcher_engine<Width, Isa>::initial() const noexcept {
  return {};
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fletcher_state<Width> fletcher_engine<Width, Isa>::update(fletcher_state<Width> state, std::span<std::byte const> data) const noexcept {
  if consteval {
    return fletcher_detail::update_bytes(state, data);
  } else {
    return fletcher_detail::from_word<Width>(
        fletcher_detail::sum_loop<isa_detail::loop_preset(Isa, fletcher_detail::own_loops)>(fletcher_detail::to_word(state), data));
  }
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr fletcher_state<Width> fletcher_engine<Width, Isa>::update(fletcher_state<Width> state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fletcher_engine<Width, Isa>::value_type fletcher_engine<Width, Isa>::finalize(fletcher_state<Width> const &state) const noexcept {
  fletcher_detail::integer<Width> const sum1 = fletcher_detail::reduce_once<Width>(state.sum1);
  fletcher_detail::integer<Width> sum2 = fletcher_detail::reduce_once<Width>(state.sum2);
  // The zero padding adds nothing to sum1, but the padded block still adds sum1 to sum2.
  if(state.block_offset % fletcher_detail::block_size<Width> != 0) {
    sum2 = fletcher_detail::reduce_once<Width>(sum2 + sum1);
  }
  return static_cast<value_type>((sum2 << (Width / 2)) | sum1);
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fletcher_state<Width> fletcher_engine<Width, Isa>::resume(value_type checksum) const noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  return {.sum1 = static_cast<sum_type>(checksum), .sum2 = static_cast<sum_type>(checksum >> (Width / 2))};
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fletcher_engine<Width, Isa>::value_type fletcher_engine<Width, Isa>::compute(std::span<std::byte const> data) const noexcept {
  return finalize(update(initial(), data));
}

template <unsigned Width, isa Isa>
  requires(Width == 16 || Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr fletcher_engine<Width, Isa>::value_type fletcher_engine<Width, Isa>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <unsigned Width, isa Isa, byte_range Range> constexpr fletcher_engine<Width, Isa>::value_type fletcher_compute(Range &&data) noexcept {
  return fletcher_engine<Width, Isa>{}.compute(std::forward<Range>(data));
}

template <unsigned Width>
  requires(Width == 16 || Width == 32 || Width == 64)
constexpr fletcher_dispatch_engine<Width>::fletcher_dispatch_engine(isa_set cpu) noexcept
    : detail::dispatch_engine<fletcher_engine<Width>, fletcher_detail::own_loops>(cpu) {}

} // namespace checksum

#endif // CHECKSUM_FLETCHER_HPP
