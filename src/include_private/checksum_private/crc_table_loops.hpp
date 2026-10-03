#ifndef CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP
#define CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP

// Run-time loops shared by src/crc/lut_*.cpp: the byte loop (crc_lut_byte, tail of slicing), slicing-by-8 and the
// kernel dispatch of crc_lut_sliced, which crc_lut_braided reuses on its first 8 slices.

#include <checksum/crc.hpp>
#include <checksum_private/crc_arch.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace checksum::crc_detail {

inline constexpr unsigned byte_mask = 0xFFU;

// Internal linkage lets compilers inline these loops fully into each source's kernel; with external linkage they keep
// an out-of-line copy.
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header): a private header, included only by src/crc/*.cpp.
namespace {

// Top byte of table[index], leading part of the next index of a non-reflected CRC. Read from the object representation:
// a byte load parallel to the entry load keeps a shift off the critical path.
template <class Register> unsigned top_byte(Register const *table, std::size_t index) noexcept {
  if constexpr(std::endian::native == std::endian::little || std::endian::native == std::endian::big) {
    constexpr std::size_t offset = std::endian::native == std::endian::little ? sizeof(Register) - 1 : 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): reading the object representation is the point.
    return std::to_integer<unsigned>(reinterpret_cast<std::byte const *>(table + index)[offset]);
  } else {
    return promoted_register<Register>{table[index]} >> (register_bits<Register> - byte_bits);
  }
}

// loop_byte() with the register kept as value ^ rest: rest and the next byte combine into next while the table load is
// in flight, and carrying next across iterations stops compilers re-associating it onto the critical path.
template <bool Reflected, class Register>
Register loop_byte_fast(Register const *table, Register remainder, std::span<std::byte const> data) noexcept {
  // The split loop costs 30-40 % on in-order cores (Cortex-M4, 16- and 32-bit registers). ponytail: pointer width
  // stands in for in-order cores; 64-bit in-order ones (A53, A55) are unmeasured, select by core if they lose there.
  if constexpr(sizeof(void *) < sizeof(std::uint64_t) && sizeof(Register) > 1) {
    return loop_byte<Reflected>(table, remainder, data);
  } else {
    constexpr unsigned top = register_bits<Register> - byte_bits;
    if(data.empty()) {
      return remainder;
    }
    promoted_register<Register> value = remainder;
    promoted_register<Register> lead = Reflected ? value & byte_mask : value >> top;
    promoted_register<Register> rest = 0;
    promoted_register<Register> next = std::to_integer<unsigned>(data[0]);
    auto const step = [&] {
      promoted_register<Register> const index = lead ^ next;
      rest = Reflected ? (value ^ rest) >> byte_bits : static_cast<Register>((value ^ rest) << byte_bits);
      value = table[index];
      lead = Reflected ? value & byte_mask : top_byte(table, index);
    };
// Clang's unrolled body re-associates next back into the chain; one step per iteration keeps it off.
#ifdef __clang__
#pragma clang loop unroll(disable)
#endif
    for(std::size_t index = 1; index < data.size(); ++index) {
      step();
      next = ((Reflected ? rest : rest >> top) ^ std::to_integer<unsigned>(data[index])) & byte_mask;
    }
    step();
    return static_cast<Register>(value ^ rest);
  }
}

// Shift of message byte index (0..7) in a 64-bit word: little endian if reflected, big endian otherwise.
template <bool Reflected> constexpr unsigned byte_shift(std::size_t index) noexcept {
  constexpr std::size_t last = (slice_count - 1) * byte_bits;
  return static_cast<unsigned>(Reflected ? byte_bits * index : last - (byte_bits * index));
}

// Slicing-by-8 over whole words, then loop_byte_fast() on the first slice for the last size % 8 bytes.
template <bool Reflected, class Register> Register loop_sliced(Register const *table, Register remainder, std::span<std::byte const> data) noexcept {
  constexpr unsigned word_bits = 64;
  constexpr std::size_t half = slice_count / 2;
  auto const align = [](promoted_register<Register> value) {
    return Reflected ? std::uint64_t{value} : std::uint64_t{value} << (word_bits - register_bits<Register>);
  };
  // remainder == low ^ high (lookups of bytes 0..3 and 4..7): two loop-carried halves stop compilers chaining all 8
  // lookups serially. Bytes i >= sizeof(Register) index straight from the message, off the critical path.
  promoted_register<Register> low = remainder;
  promoted_register<Register> high = 0;
  while(data.size() >= slice_count) {
    // The register is xored into the leading bytes; byte i indexes slice 7 - i. Fold expressions force unrolling at -O2.
    std::uint64_t const message = load_word<Reflected>(data);
    std::uint64_t const word = (message ^ align(high)) ^ align(low);
    auto const lookups = [&]<std::size_t... Index>(std::index_sequence<Index...>, auto first) {
      constexpr std::size_t offset = decltype(first)::value;
      auto const lookup = [&](auto index) {
        std::uint64_t const source = decltype(index)::value < sizeof(Register) ? word : message;
        return promoted_register<Register>{table[((slice_count - 1 - index) * slice_size) + ((source >> byte_shift<Reflected>(index)) & byte_mask)]};
      };
      return (lookup(std::integral_constant<std::size_t, offset + Index>{}) ^ ...);
    };
    low = lookups(std::make_index_sequence<half>{}, std::integral_constant<std::size_t, 0>{});
    high = lookups(std::make_index_sequence<half>{}, std::integral_constant<std::size_t, half>{});
    data = data.subspan(slice_count);
  }
  remainder = static_cast<Register>(low ^ high);
  return loop_byte_fast<Reflected>(table, remainder, data);
}

// True if run_sliced() runs the CRC32 instructions: CRC-32 or CRC-32C, reflected, std::uint32_t register.
template <bool Reflected, class Register> constexpr bool uses_crc32_instructions(Register polynomial) noexcept {
  if constexpr(Reflected && std::same_as<Register, std::uint32_t>) {
    return (crc32_instructions_available<crc32_reflected_polynomial> && polynomial == crc32_reflected_polynomial) ||
           (crc32_instructions_available<crc32c_reflected_polynomial> && polynomial == crc32c_reflected_polynomial);
  } else {
    return false;
  }
}

// At least folding_block_size bytes; only where folding_available.
template <bool Reflected, class Register>
[[gnu::always_inline]] inline Register fold_message(folding_constants const &folding, Register remainder, std::span<std::byte const> data) noexcept {
  // The kernel takes the register in 64-bit form: a non-reflected register is top-aligned.
  constexpr unsigned extension = 64 - register_bits<Register>;
  std::uint64_t const wide = Reflected ? std::uint64_t{remainder} : std::uint64_t{remainder} << extension;
  std::uint64_t const folded = fold_blocks<Reflected>(folding, wide, data);
  return static_cast<Register>(Reflected ? folded : folded >> extension);
}

// Not inlined: the call to the wide loop gives every caller a stack frame and saved registers (~20 % at 16-64 bytes on
// x86-64). Split off, the inlined fold_message() sees only inputs too short for it and drops the call.
template <bool Reflected, class Register>
[[gnu::noinline]] Register fold_long_message(folding_constants const &folding, Register remainder, std::span<std::byte const> data) noexcept {
  return fold_message<Reflected>(folding, remainder, data);
}

// Not inlined, for the same reason as fold_long_message().
template <std::uint32_t Polynomial>
[[gnu::noinline]] std::uint32_t fold_long_crc32(folding_constants const &folding, std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  return fold_crc32<Polynomial>(folding, remainder, data);
}

// CRC32 instructions; with a folding kernel, inputs of at least crc32_folding_minimum_size bytes go to fold_crc32().
template <std::uint32_t Polynomial>
std::uint32_t run_crc32_instructions(folding_constants const &folding, std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  if constexpr(folding_available && crc32_folding_minimum_size != std::numeric_limits<std::size_t>::max()) {
    if constexpr(wide_folding_minimum_size != std::numeric_limits<std::size_t>::max()) {
      if(data.size() >= wide_folding_minimum_size) {
        return fold_long_crc32<Polynomial>(folding, remainder, data);
      }
    }
    if(data.size() >= crc32_folding_minimum_size) {
      return fold_crc32<Polynomial>(folding, remainder, data);
    }
  }
  return crc32_instructions<Polynomial>(remainder, data);
}

// CRC32 instructions where they handle the parameter set, else folding from folding_minimum_size bytes, else
// loop_sliced(). table: the crc_lut_sliced table or the first 8 slices of the crc_lut_braided one.
template <bool Reflected, class Register>
[[gnu::always_inline]] inline Register run_sliced(Register const *table, folding_constants const &folding, Register polynomial, Register remainder,
                                                  std::span<std::byte const> data) noexcept {
  if constexpr(Reflected && std::same_as<Register, std::uint32_t>) {
    if constexpr(crc32_instructions_available<crc32_reflected_polynomial>) {
      if(polynomial == crc32_reflected_polynomial) {
        return run_crc32_instructions<crc32_reflected_polynomial>(folding, remainder, data);
      }
    }
    if constexpr(crc32_instructions_available<crc32c_reflected_polynomial>) {
      if(polynomial == crc32c_reflected_polynomial) {
        return run_crc32_instructions<crc32c_reflected_polynomial>(folding, remainder, data);
      }
    }
  }
  if constexpr(folding_available) {
    if constexpr(wide_folding_minimum_size != std::numeric_limits<std::size_t>::max()) {
      if(data.size() >= wide_folding_minimum_size) {
        return fold_long_message<Reflected>(folding, remainder, data);
      }
    }
    if(data.size() >= folding_minimum_size) {
      return fold_message<Reflected>(folding, remainder, data);
    }
  }
  return loop_sliced<Reflected>(table, remainder, data);
}

} // namespace
} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP
