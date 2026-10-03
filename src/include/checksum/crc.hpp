#ifndef CHECKSUM_CRC_HPP
#define CHECKSUM_CRC_HPP

#include <checksum/byte_range.hpp>

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_crc CRC
 *   CRCs of any width 1..64 over byte- and bit-granular messages, at compile time or at run time.
 *   @{
 */

namespace checksum {

/// A generator polynomial in Koopman notation: bit i is the coefficient of x^(i + 1) and the x^0 term is implicit. Every value is valid; the
/// highest set bit is the x^width term, and 0 is the polynomial 1 (width 0, rejected by every engine).
struct crc_polynomial {
  std::uint64_t value; ///< Koopman notation.

  /// Makes a polynomial from Koopman notation, e.g. CRC-16/CCITT: 0x8810. Implicit, so `.polynomial = 0x8810` works.
  constexpr explicit(false) crc_polynomial(std::uint64_t value) noexcept;

  /// Makes a polynomial from its normal form (the x^width term omitted), e.g. CRC-16/CCITT: create(0x1021, 16).
  /// @return std::nullopt if width > 64, normal_form >= 2^width, or the x^0 term is missing
  [[nodiscard]] static constexpr std::optional<crc_polynomial> create(std::uint64_t normal_form, unsigned width) noexcept;

  /// Degree of the polynomial, which is the CRC width: 0..64.
  [[nodiscard]] constexpr unsigned width() const noexcept;

  /// Normal form: bit i is the coefficient of x^i, the x^width term omitted.
  [[nodiscard]] constexpr std::uint64_t normal_form() const noexcept;

  friend constexpr bool operator==(crc_polynomial, crc_polynomial) noexcept = default;
};

/// A CRC model. A plain aggregate, usable as a template argument; crc_engine::create() validates it.
struct crc_parameters {
  crc_polynomial polynomial;       ///< Generator polynomial; its degree is the CRC width.
  std::uint64_t initial_value = 0; ///< Register value before the first message bit, unreflected.
  bool reflect_input = false;      ///< Message bytes are fed least significant bit first.
  bool reflect_output = false;     ///< The result is reflected before the final XOR.
  std::uint64_t final_xor = 0;     ///< XORed into the result, unreflected.

  friend constexpr bool operator==(crc_parameters const &, crc_parameters const &) noexcept = default;
};

/// std::uint8_t, std::uint16_t, std::uint32_t or std::uint64_t.
template <class Register>
concept crc_register = std::same_as<Register, std::uint8_t> || std::same_as<Register, std::uint16_t> || std::same_as<Register, std::uint32_t> ||
                       std::same_as<Register, std::uint64_t>;

/// The smallest crc_register with at least Width bits.
template <unsigned Width>
  requires(Width >= 1 && Width <= 64)
using crc_register_for =
    std::conditional_t<(Width <= 8), std::uint8_t,
                       std::conditional_t<(Width <= 16), std::uint16_t, std::conditional_t<(Width <= 32), std::uint32_t, std::uint64_t>>>;

/// How an engine folds whole bytes into a Register. make_table() prepares everything update() needs, once per engine; it returns
/// std::nullopt for parameters the strategy cannot handle.
template <class Strategy, class Register>
concept crc_strategy_for = crc_register<Register> && std::semiregular<typename Strategy::template table_type<Register>> &&
                           requires(crc_parameters const &parameters, typename Strategy::template table_type<Register> const &table, Register state,
                                    std::span<std::byte const> data) {
                             {
                               Strategy::template make_table<Register>(parameters)
                             } -> std::same_as<std::optional<typename Strategy::template table_type<Register>>>;
                             { Strategy::template update<Register>(table, state, data) } -> std::same_as<Register>;
                           };

struct crc_lut_none;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::crc_detail {

template <class Register> struct register_form;

} // namespace checksum::crc_detail

namespace checksum {

/// An immutable CRC engine: the strategy's table and the prepared parameters. The running CRC is a separate state_type value, so one engine
/// serves any number of computations.
template <crc_register Register, crc_strategy_for<Register> Strategy = crc_lut_none> class crc_engine {
public:
  using state_type = Register; ///< Running CRC, below 2^width.

  /// Validates parameters and builds the engine.
  /// @return std::nullopt if the width is 0 or exceeds Register, initial_value or final_xor is not below 2^width, or the strategy rejects them
  [[nodiscard]] static constexpr std::optional<crc_engine> create(crc_parameters const &parameters) noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state. Precondition: state < 2^width (asserted; masked when NDEBUG is defined).
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// Folds the first bit_length bits of data into state; the bits of a last partial byte are taken from bit 0 up if the input is reflected,
  /// else from bit 7 down. Precondition: bit_length <= 8 * data.size() (asserted; clamped when NDEBUG is defined).
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data, std::size_t bit_length) const noexcept;

  /// The CRC of state: reflected if exactly one of input and output is reflected, then XORed with the final XOR.
  [[nodiscard]] constexpr state_type finalize(state_type state) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr state_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr state_type compute(Range &&data) const noexcept;

  /// finalize(update(initial(), data, bit_length)).
  [[nodiscard]] constexpr state_type compute(std::span<std::byte const> data, std::size_t bit_length) const noexcept;

private:
  constexpr crc_engine(crc_parameters const &parameters, typename Strategy::template table_type<Register> const &table) noexcept;

  // Asserts the state precondition and masks state to the width.
  [[nodiscard]] constexpr state_type checked_state(state_type state) const noexcept;

  typename Strategy::template table_type<Register> table;
  crc_detail::register_form<Register> form; // for bit tails
  state_type initial_state;
  Register final_xor;
  Register mask; // the low width bits
  std::uint8_t width;
  bool reflect_result; // exactly one of input and output is reflected
};

/// Engine for Parameters with the smallest register, built at compile time and stored in read-only data, e.g.
/// crc_engine_for<checksum::crc32, checksum::crc_lut_byte>. Ill-formed (std::bad_optional_access) if create() rejects the parameters.
template <crc_parameters Parameters, class Strategy = crc_lut_none>
inline constexpr crc_engine<crc_register_for<Parameters.polynomial.width()>, Strategy> crc_engine_for =
    crc_engine<crc_register_for<Parameters.polynomial.width()>, Strategy>::create(Parameters).value();

/// An engine pointer plus a running state, for incremental computations. The engine must outlive the accumulator.
template <crc_register Register, crc_strategy_for<Register> Strategy = crc_lut_none> class crc_accumulator {
public:
  /// Starts a computation at engine.initial().
  constexpr explicit crc_accumulator(crc_engine<Register, Strategy> const &engine) noexcept;

  /// Temporary engines would dangle.
  crc_accumulator(crc_engine<Register, Strategy> const &&) = delete;

  /// Folds data.
  constexpr crc_accumulator &update(std::span<std::byte const> data) noexcept;

  /// Folds a byte range.
  template <byte_range Range> constexpr crc_accumulator &update(Range &&data) noexcept;

  /// Folds the first bit_length bits of data, as crc_engine::update().
  constexpr crc_accumulator &update(std::span<std::byte const> data, std::size_t bit_length) noexcept;

  /// The CRC of everything folded so far; the state is unchanged.
  [[nodiscard]] constexpr Register value() const noexcept;

  /// The running state.
  [[nodiscard]] constexpr Register state() const noexcept;

  /// Restarts at engine.initial().
  constexpr void reset() noexcept;

private:
  crc_engine<Register, Strategy> const *engine; // never null
  Register running_state;
};

} // namespace checksum

namespace checksum::crc_detail {

inline constexpr std::size_t no_table_size = 0;
inline constexpr std::size_t nibble_size = 16;
inline constexpr std::size_t slice_size = 256; // entries of one byte table (slice)
inline constexpr std::size_t slice_count = 8;  // slices of the slicing-by-8 table
inline constexpr std::size_t word_size = 8;    // bytes per braid word

// Slice k at [k * 256, (k + 1) * 256).
inline constexpr std::size_t sliced_table_size = slice_count * slice_size;

// The slicing-by-8 slices, then one braid slice per byte of a braid word.
inline constexpr std::size_t braided_table_size = (slice_count + word_size) * slice_size;

template <class Register, std::size_t Size> struct lookup_table;

// The built-in strategies differ only in the size of their table.
template <std::size_t Size> struct lookup_strategy {
  template <crc_register Register> using table_type = lookup_table<Register, Size>;

  // std::nullopt if the width does not fit Register.
  template <crc_register Register>
  [[nodiscard]] static constexpr std::optional<table_type<Register>> make_table(crc_parameters const &parameters) noexcept;

  // Constant evaluation runs the bitwise, nibble or byte-table loop, run time the out-of-line update_loop().
  template <crc_register Register>
  [[nodiscard]] static constexpr Register update(table_type<Register> const &table, Register state, std::span<std::byte const> data) noexcept;
};

} // namespace checksum::crc_detail

namespace checksum {

/// Bitwise, no table: the smallest code and data (the default).
struct crc_lut_none : crc_detail::lookup_strategy<crc_detail::no_table_size> {};

/// 16-entry table, two lookups per byte.
struct crc_lut_nibble : crc_detail::lookup_strategy<crc_detail::nibble_size> {};

/// 256-entry table, one lookup per byte.
struct crc_lut_byte : crc_detail::lookup_strategy<crc_detail::slice_size> {};

/// Slicing-by-8: 8 x 256 entries, one 8-byte word per iteration. Uses the CPU's CRC and carry-less multiplication instructions where the
/// compiler flags enable them.
struct crc_lut_sliced : crc_detail::lookup_strategy<crc_detail::sliced_table_size> {};

/// crc_lut_sliced plus 8 braid slices (16 x 256 entries): five interleaved streams of 8-byte words from 128 bytes on. Builds with
/// acceleration run the crc_lut_sliced path instead.
struct crc_lut_braided : crc_detail::lookup_strategy<crc_detail::braided_table_size> {};

} // namespace checksum

namespace checksum::crc_detail {

// Reverses the low width (1..64) bits of value and drops the others.
constexpr std::uint64_t reflect(std::uint64_t value, unsigned width) noexcept;

template <class Register> inline constexpr unsigned register_bits = std::numeric_limits<Register>::digits;

// Register promoted to at least unsigned int, so that bit operations never act on signed values.
template <class Register> using promoted_register = decltype(Register{} | 0U);

inline constexpr unsigned byte_bits = 8;
inline constexpr unsigned nibble_bits = 4;
inline constexpr unsigned nibble_mask = 0x0FU;
inline constexpr std::size_t braid_count = 5; // interleaved streams of crc_lut_braided

// Constants for the low and the high 64 bits of a 128-bit block, in vector lane order.
using folding_pair = std::array<std::uint64_t, 2>;

// Carry-less multiplication constants of a parameter set (see make_folding_constants()). Aligned so that each pair is one aligned vector
// load.
struct alignas(16) folding_constants {
  folding_pair by_sixteen;  // fold over 16 blocks (2048 bits)
  folding_pair by_eight;    // fold over 8 blocks
  folding_pair by_four;     // fold over 4 blocks
  folding_pair by_one;      // fold over 1 block (128 bits)
  std::uint64_t quotient;   // Barrett constant floor(x^128 / Q) without its x^64 term
  std::uint64_t polynomial; // Q without its x^64 term
};

// The slicing-by-8 and braided tables hold folding constants.
template <std::size_t Size> inline constexpr bool has_folding = Size >= sliced_table_size;

// The polynomial prepared for a register. Reflected CRCs use the public state as is; the others run top-aligned, shifted left by shift on
// entry and right on exit of every update, so every loop works for every width that fits Register.
template <class Register> struct register_form {
  Register polynomial = 0; // reflected, or normal and shifted left by shift
  std::uint8_t shift = 0;  // register_bits - width if not reflected, else 0
  bool reflected = false;
};

template <class Register, std::size_t Size> struct lookup_table {
  register_form<Register> form;
  std::array<Register, Size> entries{};
};

// crc_lut_none: the register form only.
template <class Register> struct lookup_table<Register, no_table_size> {
  register_form<Register> form;
};

template <class Register, std::size_t Size>
  requires has_folding<Size>
struct lookup_table<Register, Size> {
  register_form<Register> form;
  std::array<Register, Size> entries{};
  folding_constants folding{};
};

// True if the width is 1..register_bits<Register>.
template <class Register> constexpr bool fits_register(crc_parameters const &parameters) noexcept;

template <class Register> constexpr register_form<Register> make_register_form(crc_parameters const &parameters) noexcept;

// Shifts one message bit, already XORed into the register's input end, through the register without a branch.
template <bool Reflected, class Register> constexpr Register bitwise_step(Register remainder, Register polynomial) noexcept;

// XORs value_bits message bits into the register's input end: the low end if reflected, the high end otherwise.
template <bool Reflected, class Register> constexpr Register xor_input(Register remainder, unsigned value, unsigned value_bits) noexcept;

template <bool Reflected, class Register> constexpr Register bitwise_byte(Register remainder, Register polynomial, std::byte data) noexcept;

template <bool Reflected, class Register>
constexpr Register loop_none(Register polynomial, Register remainder, std::span<std::byte const> data) noexcept;

// One lookup in a table of 2^Bits entries (Bits 4 or 8).
template <bool Reflected, unsigned Bits, class Register>
constexpr Register table_step(Register const *table, Register remainder, unsigned value) noexcept;

template <bool Reflected, class Register>
constexpr Register loop_nibble(Register const *table, Register remainder, std::span<std::byte const> data) noexcept;

// Also the constant-evaluation loop of crc_lut_sliced and crc_lut_braided.
template <bool Reflected, class Register>
constexpr Register loop_byte(Register const *table, Register remainder, std::span<std::byte const> data) noexcept;

// Slice k of the slicing-by-8 table folds a byte followed by k zero bytes; braid slice 8 + k a byte followed by
// braid_count * word_size - 1 - k zero bytes.
template <std::size_t Size, bool Reflected, class Register> constexpr std::array<Register, Size> table_entries(Register polynomial) noexcept;

// x^exponent modulo x^64 + polynomial, normal bit order.
constexpr std::uint64_t power_modulo(std::uint64_t polynomial, unsigned exponent) noexcept;

// Every width W runs in a 64-bit register modulo Q = x^(64 - W) * (x^W + polynomial). The constants are x^distance mod Q for each fold
// distance, the Barrett constant and Q, in the bit order of the CRC.
constexpr folding_constants make_folding_constants(crc_parameters const &parameters) noexcept;

// Runs fold(std::bool_constant<reflected>, register) on the internal (top-aligned if not reflected) form of a public state.
template <class Register, class Fold> constexpr Register fold_internal(register_form<Register> const &form, Register state, Fold fold) noexcept;

// Folds count (<= 8) bits of tail bitwise: bits 0.. if reflected, else bits 7.. down.
template <class Register>
constexpr Register update_tail(register_form<Register> const &form, Register state, std::byte tail, unsigned count) noexcept;

// Run-time loops, one per built-in strategy, defined and explicitly instantiated for every register type and bit order in
// src/crc/lut_*.cpp. They take and return the internal register.
template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, no_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept;
template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, nibble_size> const &table, Register remainder, std::span<std::byte const> data) noexcept;
template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, slice_size> const &table, Register remainder, std::span<std::byte const> data) noexcept;
template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, sliced_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept;
template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, braided_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept;

} // namespace checksum::crc_detail

///@}
///@}

namespace checksum::crc_detail {

constexpr std::uint64_t reflect(std::uint64_t value, unsigned width) noexcept {
  constexpr std::uint64_t nibbles = 0x0F0F0F0F0F0F0F0FULL;
  constexpr std::uint64_t pairs = 0x3333333333333333ULL;
  constexpr std::uint64_t bits = 0x5555555555555555ULL;
  value = std::byteswap(value);
  value = ((value >> 4U) & nibbles) | ((value & nibbles) << 4U);
  value = ((value >> 2U) & pairs) | ((value & pairs) << 2U);
  value = ((value >> 1U) & bits) | ((value & bits) << 1U);
  return value >> (64U - width);
}

} // namespace checksum::crc_detail

namespace checksum {

constexpr crc_polynomial::crc_polynomial(std::uint64_t value) noexcept : value(value) {}

constexpr std::optional<crc_polynomial> crc_polynomial::create(std::uint64_t normal_form, unsigned width) noexcept {
  if(width > 64 || std::cmp_greater(std::bit_width(normal_form), width) || (width != 0 && (normal_form & 1U) == 0)) {
    return std::nullopt;
  }
  return width == 0 ? crc_polynomial(0) : crc_polynomial((normal_form >> 1U) | (std::uint64_t{1} << (width - 1U)));
}

constexpr unsigned crc_polynomial::width() const noexcept { return static_cast<unsigned>(std::bit_width(value)); }

constexpr std::uint64_t crc_polynomial::normal_form() const noexcept {
  // The full polynomial is value * x + 1; the normal form drops its x^width term (shifted out for width 64).
  return value == 0 ? 0 : ((value << 1U) | 1U) ^ (std::bit_floor(value) << 1U);
}

} // namespace checksum

namespace checksum::crc_detail {

template <class Register> constexpr bool fits_register(crc_parameters const &parameters) noexcept {
  return parameters.polynomial.width() >= 1 && parameters.polynomial.width() <= register_bits<Register>;
}

template <class Register> constexpr register_form<Register> make_register_form(crc_parameters const &parameters) noexcept {
  unsigned const width = parameters.polynomial.width();
  bool const reflected = parameters.reflect_input;
  auto const shift = static_cast<std::uint8_t>(reflected ? 0U : register_bits<Register> - width);
  std::uint64_t const normal_form = parameters.polynomial.normal_form();
  return {
      .polynomial = static_cast<Register>(reflected ? reflect(normal_form, width) : normal_form << shift), .shift = shift, .reflected = reflected};
}

template <bool Reflected, class Register> constexpr Register bitwise_step(Register remainder, Register polynomial) noexcept {
  promoted_register<Register> const value = remainder;
  promoted_register<Register> const feedback = Reflected ? value & 1U : value >> (register_bits<Register> - 1U);
  promoted_register<Register> const shifted = Reflected ? value >> 1U : value << 1U;
  return static_cast<Register>(shifted ^ (promoted_register<Register>{polynomial} & (0U - feedback)));
}

template <bool Reflected, class Register> constexpr Register xor_input(Register remainder, unsigned value, unsigned value_bits) noexcept {
  if constexpr(Reflected) {
    return static_cast<Register>(promoted_register<Register>{remainder} ^ value);
  } else {
    return static_cast<Register>(promoted_register<Register>{remainder} ^
                                 (promoted_register<Register>{value} << (register_bits<Register> - value_bits)));
  }
}

template <bool Reflected, class Register> constexpr Register bitwise_byte(Register remainder, Register polynomial, std::byte data) noexcept {
  remainder = xor_input<Reflected>(remainder, std::to_integer<unsigned>(data), byte_bits);
  for(unsigned bit = 0; bit < byte_bits; ++bit) {
    remainder = bitwise_step<Reflected>(remainder, polynomial);
  }
  return remainder;
}

template <bool Reflected, class Register>
constexpr Register loop_none(Register polynomial, Register remainder, std::span<std::byte const> data) noexcept {
  for(std::byte const byte : data) {
    remainder = bitwise_byte<Reflected>(remainder, polynomial, byte);
  }
  return remainder;
}

template <bool Reflected, unsigned Bits, class Register>
constexpr Register table_step(Register const *table, Register remainder, unsigned value) noexcept {
  promoted_register<Register> const current = remainder;
  constexpr unsigned index_mask = (1U << Bits) - 1U;
  if constexpr(Reflected) {
    return static_cast<Register>(promoted_register<Register>{table[(current ^ value) & index_mask]} ^ (current >> Bits));
  } else {
    return static_cast<Register>(promoted_register<Register>{table[(current >> (register_bits<Register> - Bits)) ^ value]} ^ (current << Bits));
  }
}

template <bool Reflected, class Register>
constexpr Register loop_nibble(Register const *table, Register remainder, std::span<std::byte const> data) noexcept {
  for(std::byte const byte : data) {
    auto const value = std::to_integer<unsigned>(byte);
    unsigned const first = Reflected ? value & nibble_mask : value >> nibble_bits;
    unsigned const second = Reflected ? value >> nibble_bits : value & nibble_mask;
    remainder = table_step<Reflected, nibble_bits>(table, table_step<Reflected, nibble_bits>(table, remainder, first), second);
  }
  return remainder;
}

template <bool Reflected, class Register>
constexpr Register loop_byte(Register const *table, Register remainder, std::span<std::byte const> data) noexcept {
  for(std::byte const byte : data) {
    remainder = table_step<Reflected, byte_bits>(table, remainder, std::to_integer<unsigned>(byte));
  }
  return remainder;
}

template <std::size_t Size, bool Reflected, class Register> constexpr std::array<Register, Size> table_entries(Register polynomial) noexcept {
  std::array<Register, Size> table{};
  if constexpr(Size == nibble_size) {
    for(unsigned index = 0; index < Size; ++index) {
      Register remainder = xor_input<Reflected>(Register{0}, index, nibble_bits);
      for(unsigned bit = 0; bit < nibble_bits; ++bit) {
        remainder = bitwise_step<Reflected>(remainder, polynomial);
      }
      table[index] = remainder;
    }
  } else {
    for(std::size_t index = 0; index < slice_size; ++index) {
      table[index] = bitwise_byte<Reflected>(Register{0}, polynomial, static_cast<std::byte>(index));
    }
    if constexpr(Size >= sliced_table_size) {
      for(std::size_t index = slice_size; index < sliced_table_size; ++index) {
        table[index] = table_step<Reflected, byte_bits>(table.data(), table[index - slice_size], 0U);
      }
    }
    if constexpr(Size == braided_table_size) {
      // advanced[n]: byte n followed by `zeros` zero bytes, starting from slice 7 (7 zero bytes); braid slice k
      // (slice 8 + k) needs braid_count * word_size - 1 - k of them.
      std::array<Register, slice_size> advanced{};
      for(std::size_t index = 0; index < slice_size; ++index) {
        advanced[index] = table[sliced_table_size - slice_size + index];
      }
      constexpr std::size_t block_size = braid_count * word_size;
      for(std::size_t zeros = slice_count; zeros < block_size; ++zeros) {
        for(Register &entry : advanced) {
          entry = table_step<Reflected, byte_bits>(table.data(), entry, 0U);
        }
        if(zeros >= block_size - word_size) {
          for(std::size_t index = 0; index < slice_size; ++index) {
            table[((slice_count + block_size - 1 - zeros) * slice_size) + index] = advanced[index];
          }
        }
      }
    }
  }
  return table;
}

constexpr std::uint64_t power_modulo(std::uint64_t polynomial, unsigned exponent) noexcept {
  constexpr unsigned top_bit = 63;
  std::uint64_t remainder = 1;
  for(unsigned power = 0; power < exponent; ++power) {
    remainder = (remainder << 1U) ^ (polynomial & (0U - (remainder >> top_bit)));
  }
  return remainder;
}

constexpr folding_constants make_folding_constants(crc_parameters const &parameters) noexcept {
  constexpr unsigned register_width = 64;
  constexpr unsigned top_bit = register_width - 1;
  constexpr unsigned block_bits = 128;
  std::uint64_t const polynomial = parameters.polynomial.normal_form() << (register_width - parameters.polynomial.width());
  // floor(x^128 / Q): x^(64 + k) = quotient_k * Q + remainder_k, starting from x^64 = 1 * Q + polynomial; the leading 1
  // becomes the x^64 term, which is not stored.
  std::uint64_t quotient = 0;
  std::uint64_t remainder = polynomial;
  for(unsigned bit = 0; bit < register_width; ++bit) {
    std::uint64_t const top = remainder >> top_bit;
    quotient = (quotient << 1U) | top;
    remainder = (remainder << 1U) ^ (polynomial & (0U - top));
  }
  auto const fold_pair = [&](unsigned distance) -> folding_pair {
    if(!parameters.reflect_input) {
      // The high half H of a block is in the high lane: H * x^64 * x^distance; the low half L: L * x^distance.
      return {power_modulo(polynomial, distance), power_modulo(polynomial, distance + register_width)};
    }
    // Reflected: the high half of a block is in the low lane, and the product of two reflected values is the reflected
    // product times x, so every fold constant is one power of x lower.
    return {reflect(power_modulo(polynomial, distance + register_width - 1), register_width),
            reflect(power_modulo(polynomial, distance - 1), register_width)};
  };
  return {.by_sixteen = fold_pair(16 * block_bits),
          .by_eight = fold_pair(8 * block_bits),
          .by_four = fold_pair(4 * block_bits),
          .by_one = fold_pair(block_bits),
          .quotient = parameters.reflect_input ? reflect(quotient, register_width) : quotient,
          .polynomial = parameters.reflect_input ? reflect(polynomial, register_width) : polynomial};
}

template <class Register, class Fold> constexpr Register fold_internal(register_form<Register> const &form, Register state, Fold fold) noexcept {
  if(form.reflected) {
    return fold(std::true_type{}, state);
  }
  return static_cast<Register>(
      promoted_register<Register>{fold(std::false_type{}, static_cast<Register>(promoted_register<Register>{state} << form.shift))} >> form.shift);
}

template <class Register>
constexpr Register update_tail(register_form<Register> const &form, Register state, std::byte tail, unsigned count) noexcept {
  auto const value = std::to_integer<unsigned>(tail);
  return fold_internal(form, state, [&](auto reflected, Register remainder) -> Register {
    constexpr bool is_reflected = decltype(reflected)::value;
    for(unsigned index = 0; index < count; ++index) {
      unsigned const bit = (value >> (is_reflected ? index : byte_bits - 1U - index)) & 1U;
      remainder = bitwise_step<is_reflected>(xor_input<is_reflected>(remainder, bit, 1U), form.polynomial);
    }
    return remainder;
  });
}

} // namespace checksum::crc_detail

namespace checksum {

template <std::size_t Size>
template <crc_register Register>
constexpr std::optional<typename crc_detail::lookup_strategy<Size>::template table_type<Register>>
crc_detail::lookup_strategy<Size>::make_table(crc_parameters const &parameters) noexcept {
  if(!crc_detail::fits_register<Register>(parameters)) {
    return std::nullopt;
  }
  auto const form = crc_detail::make_register_form<Register>(parameters);
  if constexpr(Size == crc_detail::no_table_size) {
    return crc_detail::lookup_table<Register, Size>{.form = form};
  } else {
    crc_detail::lookup_table<Register, Size> table{.form = form,
                                                   .entries = form.reflected ? crc_detail::table_entries<Size, true>(form.polynomial)
                                                                             : crc_detail::table_entries<Size, false>(form.polynomial)};
    if constexpr(crc_detail::has_folding<Size>) {
      table.folding = crc_detail::make_folding_constants(parameters);
    }
    return table;
  }
}

template <std::size_t Size>
template <crc_register Register>
constexpr Register crc_detail::lookup_strategy<Size>::update(table_type<Register> const &table, Register state,
                                                             std::span<std::byte const> data) noexcept {
  return crc_detail::fold_internal(table.form, state, [&](auto reflected, Register remainder) -> Register {
    if consteval {
      if constexpr(Size == crc_detail::no_table_size) {
        return crc_detail::loop_none<decltype(reflected)::value>(table.form.polynomial, remainder, data);
      } else if constexpr(Size == crc_detail::nibble_size) {
        return crc_detail::loop_nibble<decltype(reflected)::value>(table.entries.data(), remainder, data);
      } else {
        return crc_detail::loop_byte<decltype(reflected)::value>(table.entries.data(), remainder, data);
      }
    } else {
      return crc_detail::update_loop<decltype(reflected)::value>(table, remainder, data);
    }
  });
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr crc_engine<Register, Strategy>::crc_engine(crc_parameters const &parameters,
                                                     typename Strategy::template table_type<Register> const &table) noexcept
    : table(table), form(crc_detail::make_register_form<Register>(parameters)),
      initial_state(static_cast<Register>(form.reflected ? crc_detail::reflect(parameters.initial_value, parameters.polynomial.width())
                                                         : parameters.initial_value)),
      final_xor(static_cast<Register>(parameters.final_xor)), mask(static_cast<Register>(~std::uint64_t{0} >> (64U - parameters.polynomial.width()))),
      width(static_cast<std::uint8_t>(parameters.polynomial.width())), reflect_result(form.reflected != parameters.reflect_output) {}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::checked_state(state_type state) const noexcept {
  assert(state <= mask);
  return static_cast<Register>(state & mask);
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr std::optional<crc_engine<Register, Strategy>> crc_engine<Register, Strategy>::create(crc_parameters const &parameters) noexcept {
  if(!crc_detail::fits_register<Register>(parameters)) {
    return std::nullopt;
  }
  unsigned const width = parameters.polynomial.width();
  if(std::cmp_greater(std::bit_width(parameters.initial_value), width) || std::cmp_greater(std::bit_width(parameters.final_xor), width)) {
    return std::nullopt;
  }
  auto const table = Strategy::template make_table<Register>(parameters);
  if(!table) {
    return std::nullopt;
  }
  return crc_engine(parameters, *table);
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::initial() const noexcept {
  return initial_state;
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::update(state_type state,
                                                                                                     std::span<std::byte const> data) const noexcept {
  return Strategy::template update<Register>(table, checked_state(state), data);
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
template <byte_range Range>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::update(state_type state, Range &&data) const noexcept {
  state = checked_state(state);
  detail::for_each_chunk(std::forward<Range>(data),
                         [&](std::span<std::byte const> chunk) { state = Strategy::template update<Register>(table, state, chunk); });
  return state;
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type
crc_engine<Register, Strategy>::update(state_type state, std::span<std::byte const> data, std::size_t bit_length) const noexcept {
  std::size_t bytes = bit_length / 8;
  auto bits = static_cast<unsigned>(bit_length % 8);
  bool const in_range = bytes < data.size() || (bytes == data.size() && bits == 0);
  assert(in_range);
  if(!in_range) {
    bytes = data.size();
    bits = 0;
  }
  state = update(state, data.first(bytes));
  return bits == 0 ? state : crc_detail::update_tail(form, state, data[bytes], bits);
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::finalize(state_type state) const noexcept {
  state = checked_state(state);
  if(reflect_result) {
    state = static_cast<Register>(crc_detail::reflect(state, width));
  }
  return static_cast<Register>(state ^ final_xor);
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type
crc_engine<Register, Strategy>::compute(std::span<std::byte const> data) const noexcept {
  return finalize(update(initial(), data));
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
template <byte_range Range>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr typename crc_engine<Register, Strategy>::state_type crc_engine<Register, Strategy>::compute(std::span<std::byte const> data,
                                                                                                      std::size_t bit_length) const noexcept {
  return finalize(update(initial(), data, bit_length));
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr crc_accumulator<Register, Strategy>::crc_accumulator(crc_engine<Register, Strategy> const &engine) noexcept
    : engine(&engine), running_state(engine.initial()) {}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr crc_accumulator<Register, Strategy> &crc_accumulator<Register, Strategy>::update(std::span<std::byte const> data) noexcept {
  running_state = engine->update(running_state, data);
  return *this;
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
template <byte_range Range>
constexpr crc_accumulator<Register, Strategy> &crc_accumulator<Register, Strategy>::update(Range &&data) noexcept {
  running_state = engine->update(running_state, std::forward<Range>(data));
  return *this;
}

template <crc_register Register, crc_strategy_for<Register> Strategy>
constexpr crc_accumulator<Register, Strategy> &crc_accumulator<Register, Strategy>::update(std::span<std::byte const> data,
                                                                                           std::size_t bit_length) noexcept {
  running_state = engine->update(running_state, data, bit_length);
  return *this;
}

template <crc_register Register, crc_strategy_for<Register> Strategy> constexpr Register crc_accumulator<Register, Strategy>::value() const noexcept {
  return engine->finalize(running_state);
}

template <crc_register Register, crc_strategy_for<Register> Strategy> constexpr Register crc_accumulator<Register, Strategy>::state() const noexcept {
  return running_state;
}

template <crc_register Register, crc_strategy_for<Register> Strategy> constexpr void crc_accumulator<Register, Strategy>::reset() noexcept {
  running_state = engine->initial();
}

} // namespace checksum

#endif // CHECKSUM_CRC_HPP
