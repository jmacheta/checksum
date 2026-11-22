#ifndef CHECKSUM_FNV1A_HPP
#define CHECKSUM_FNV1A_HPP

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
 *   @defgroup checksum_fnv1a FNV-1a
 *   The non-cryptographic hashes FNV-1a 32 and FNV-1a 64 of Fowler, Noll and Vo, at compile time or at run time.
 *   @{
 */

namespace checksum::fnv1a_detail {

// The hash word, offset basis and prime of each width; the primes are 2^24 + 2^8 + 0x93 and 2^40 + 2^8 + 0xB3.
template <unsigned Width> struct parameters;

template <> struct parameters<32> {
  using word = std::uint32_t;
  static constexpr word offset_basis = 0x811C9DC5U;
  static constexpr word prime = 0x01000193U;
};

template <> struct parameters<64> {
  using word = std::uint64_t;
  static constexpr word offset_basis = 0xCBF29CE484222325U;
  static constexpr word prime = 0x00000100000001B3U;
};

} // namespace checksum::fnv1a_detail

namespace checksum {

/// Running FNV-1a hash of Width 32 or 64 bits. Every value is valid; the default is the empty message.
template <unsigned Width>
  requires(Width == 32 || Width == 64)
struct fnv1a_state {
  /// The hash of the bytes folded so far; the offset basis for the empty message.
  fnv1a_detail::parameters<Width>::word hash = fnv1a_detail::parameters<Width>::offset_basis;

  friend constexpr bool operator==(fnv1a_state, fnv1a_state) noexcept = default;
};

/// FNV-1a of Width 32 or 64 bits. Every instruction-set preset Isa (checksum/isa.hpp) runs the same portable loop. Empty: make one
/// wherever it is needed.
template <unsigned Width, isa Isa = isa::generic>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
class fnv1a_engine {
public:
  using state_type = fnv1a_state<Width>;         ///< Running hash.
  using value_type = decltype(state_type::hash); ///< The hash: std::uint32_t or std::uint64_t.

  /// generic, whose loop every preset runs.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The hash of state.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// State of a message whose hash is hash: the hash is the whole state.
  [[nodiscard]] constexpr state_type resume(value_type hash) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// fnv1a_engine<Width, Isa>{}.compute(data).
template <unsigned Width, isa Isa = isa::generic, byte_range Range>
[[nodiscard]] constexpr fnv1a_engine<Width, Isa>::value_type fnv1a_compute(Range &&data) noexcept;

} // namespace checksum

namespace checksum::fnv1a_detail {

// The presets a dispatch engine chooses from: generic alone, whose loop every preset runs.
inline constexpr isa_set own_loops = isa_set{};

} // namespace checksum::fnv1a_detail

namespace checksum {

/// An fnv1a_engine with the dispatch engines' interface. No preset has a loop of its own, so it always chooses generic.
template <unsigned Width>
  requires(Width == 32 || Width == 64)
class fnv1a_dispatch_engine : public detail::dispatch_engine<fnv1a_engine<Width>, fnv1a_detail::own_loops> {
public:
  /// cpu does not change the loop.
  constexpr explicit fnv1a_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::fnv1a_detail {

template <unsigned Width> using word = parameters<Width>::word;

// One byte into hash. Header-only and inline: each byte depends on the previous one, so no kernel or out-of-line loop speeds it up.
template <unsigned Width> constexpr word<Width> mix(word<Width> hash, std::byte byte) noexcept;

// Four bytes per iteration: on in-order cores the loop of one byte took as long as the hash (7 cycles per byte on a Cortex-M4).
template <unsigned Width> constexpr word<Width> fold(word<Width> hash, std::span<std::byte const> data) noexcept;

// FNV-1a 64 in 32-bit halves, for AArch64 from 64 bytes: the low half of the prime is 0x1B3, so a byte waits for one
// 32 x 32 -> 64 multiplication instead of a 64-bit one. A Cortex-A72 runs 298 instead of 249 MB/s, but hashes many short
// inputs faster with the single multiplication; x86-64 is slower with the halves.
constexpr std::uint64_t fold_halves(std::uint64_t hash, std::span<std::byte const> data) noexcept;

} // namespace checksum::fnv1a_detail

///@}
///@}

namespace checksum::fnv1a_detail {

template <unsigned Width> constexpr word<Width> mix(word<Width> hash, std::byte byte) noexcept {
  return static_cast<word<Width>>((hash ^ std::to_integer<word<Width>>(byte)) * parameters<Width>::prime);
}

template <unsigned Width> constexpr word<Width> fold(word<Width> hash, std::span<std::byte const> data) noexcept {
#ifdef __aarch64__
  if constexpr(Width == 64) {
    if(data.size() >= 64) {
      return fold_halves(hash, data);
    }
  }
#endif
  std::byte const *position = data.data();
  std::size_t size = data.size();
  for(; size >= 4; size -= 4, position += 4) {
    hash = mix<Width>(mix<Width>(mix<Width>(mix<Width>(hash, position[0]), position[1]), position[2]), position[3]);
  }
  for(; size != 0; --size, ++position) {
    hash = mix<Width>(hash, *position);
  }
  return hash;
}

constexpr std::uint64_t fold_halves(std::uint64_t hash, std::span<std::byte const> data) noexcept {
  constexpr std::uint32_t low_prime = 0x1B3;
  static_assert(parameters<64>::prime == (1ULL << 40U) + low_prime);
  auto low = static_cast<std::uint32_t>(hash);
  auto high = static_cast<std::uint32_t>(hash >> 32U);
  for(std::byte const byte : data) {
    low ^= std::to_integer<std::uint32_t>(byte);
    std::uint64_t const product = std::uint64_t{low} * low_prime;
    high = (high * low_prime) + (low << 8U) + static_cast<std::uint32_t>(product >> 32U);
    low = static_cast<std::uint32_t>(product);
  }
  return (std::uint64_t{high} << 32U) | low;
}

} // namespace checksum::fnv1a_detail

namespace checksum {

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr isa fnv1a_engine<Width, Isa>::preset() noexcept {
  return isa_detail::loop_preset(Isa, fnv1a_detail::own_loops);
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fnv1a_state<Width> fnv1a_engine<Width, Isa>::initial() const noexcept {
  return {};
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fnv1a_state<Width> fnv1a_engine<Width, Isa>::update(fnv1a_state<Width> state, std::span<std::byte const> data) const noexcept {
  return {.hash = fnv1a_detail::fold<Width>(state.hash, data)};
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr fnv1a_state<Width> fnv1a_engine<Width, Isa>::update(fnv1a_state<Width> state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fnv1a_engine<Width, Isa>::value_type fnv1a_engine<Width, Isa>::finalize(fnv1a_state<Width> const &state) const noexcept {
  return state.hash;
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fnv1a_state<Width> fnv1a_engine<Width, Isa>::resume(value_type hash) const noexcept {
  return {.hash = hash};
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
constexpr fnv1a_engine<Width, Isa>::value_type fnv1a_engine<Width, Isa>::compute(std::span<std::byte const> data) const noexcept {
  return fnv1a_detail::fold<Width>(initial().hash, data);
}

template <unsigned Width, isa Isa>
  requires(Width == 32 || Width == 64) && (isa_available(Isa))
template <byte_range Range>
constexpr fnv1a_engine<Width, Isa>::value_type fnv1a_engine<Width, Isa>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <unsigned Width, isa Isa, byte_range Range> constexpr fnv1a_engine<Width, Isa>::value_type fnv1a_compute(Range &&data) noexcept {
  return fnv1a_engine<Width, Isa>{}.compute(std::forward<Range>(data));
}

template <unsigned Width>
  requires(Width == 32 || Width == 64)
constexpr fnv1a_dispatch_engine<Width>::fnv1a_dispatch_engine(isa_set cpu) noexcept
    : detail::dispatch_engine<fnv1a_engine<Width>, fnv1a_detail::own_loops>(cpu) {}

} // namespace checksum

#endif // CHECKSUM_FNV1A_HPP
