#ifndef CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP
#define CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP

// Run-time loops of the table strategies for one preset: the byte loop (crc_lut_byte, tail of slicing), slicing-by-8,
// braiding and the kernel dispatch of crc_lut_sliced, which crc_lut_braided reuses on its first 8 slices. A source of src/crc/
// defines CHECKSUM_PRESET (see target_begin.hpp) and includes this header once; CHECKSUM_CRC_SLICED_ENTRY or
// CHECKSUM_CRC_BRAIDED_ENTRY instantiate that strategy's update_loop<isa::CHECKSUM_PRESET>().

// Every std header of this file and of the architecture headers comes before the target region, so that their inline
// functions compile for the build's own target.
#include <checksum/crc.hpp>
#include <checksum/isa.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include <checksum_private/target_begin.hpp>

#include <checksum_private/crc_arch.hpp>

namespace checksum::crc_detail {

inline constexpr unsigned byte_mask = 0xFFU;

// Internal linkage lets compilers inline these loops fully into each source's kernel; with external linkage they keep
// an out-of-line copy.
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header,cert-dcl59-cpp): a private header, included only by src/crc/*.cpp.
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
  // The split loop costs 30-40 % on in-order cores (Cortex-M4, 16- and 32-bit registers). 32-bit targets stand in for
  // in-order cores; on 64-bit in-order ones (Cortex-A53, A55) it is unmeasured.
  if constexpr(target_detail::word32_target && sizeof(Register) > 1) {
    return loop_byte<Reflected>(table, remainder, data);
  } else {
    constexpr unsigned top = register_bits<Register> - byte_bits;
    if(data.empty()) {
      return remainder;
    }
    promoted_register<Register> value = remainder;
    promoted_register<Register> lead = Reflected ? value & byte_mask : value >> top;
    promoted_register<Register> rest = 0;
    promoted_register<Register> next = std::to_integer<unsigned>(data[0]); // NOLINT(hicpp-use-auto,modernize-use-auto): wider than unsigned
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
template <isa Isa, bool Reflected, class Register> constexpr bool uses_crc32_instructions(Register polynomial) noexcept {
  if constexpr(Reflected && std::same_as<Register, std::uint32_t>) {
    return (crc32_kernel<Isa, crc32_reflected_polynomial>::available && polynomial == crc32_reflected_polynomial) ||
           (crc32_kernel<Isa, crc32c_reflected_polynomial>::available && polynomial == crc32c_reflected_polynomial);
  } else {
    return false;
  }
}

// At least folding_block_size bytes; only where folding_kernel<Isa>::available.
template <isa Isa, bool Reflected, class Register>
CHECKSUM_ALWAYS_INLINE inline Register fold_message(folding_constants const &folding, Register remainder, std::span<std::byte const> data) noexcept {
  // The kernel takes the register in 64-bit form: a non-reflected register is top-aligned.
  constexpr unsigned extension = 64 - register_bits<Register>;
  std::uint64_t const wide = Reflected ? std::uint64_t{remainder} : std::uint64_t{remainder} << extension;
  std::uint64_t const folded = folding_kernel<Isa>::template fold<Reflected>(folding, wide, data);
  return static_cast<Register>(Reflected ? folded : folded >> extension);
}

// Not inlined: the call to the wide loop gives every caller a stack frame and saved registers (~20 % at 16-64 bytes on
// x86-64). Split off, the inlined fold_message() sees only inputs too short for it and drops the call.
template <isa Isa, bool Reflected, class Register>
CHECKSUM_NOINLINE Register fold_long_message(folding_constants const &folding, Register remainder, std::span<std::byte const> data) noexcept {
  return fold_message<Isa, Reflected>(folding, remainder, data);
}

// Not inlined, for the same reason as fold_long_message().
template <isa Isa, std::uint32_t Polynomial>
CHECKSUM_NOINLINE std::uint32_t fold_long_crc32(folding_constants const &folding, std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  return crc32_kernel<Isa, Polynomial>::fold(folding, remainder, data);
}

// CRC32 instructions; with a folding kernel, inputs of at least their folding_minimum_size bytes go to crc32_kernel::fold().
template <isa Isa, std::uint32_t Polynomial>
std::uint32_t run_crc32_instructions(folding_constants const &folding, std::uint32_t remainder, std::span<std::byte const> data) noexcept {
  using instructions = crc32_kernel<Isa, Polynomial>;
  if constexpr(folding_kernel<Isa>::available && instructions::folding_minimum_size != std::numeric_limits<std::size_t>::max()) {
    if constexpr(folding_kernel<Isa>::wide_minimum_size != std::numeric_limits<std::size_t>::max()) {
      if(data.size() >= folding_kernel<Isa>::wide_minimum_size) {
        return fold_long_crc32<Isa, Polynomial>(folding, remainder, data);
      }
    }
    if(data.size() >= instructions::folding_minimum_size) {
      return instructions::fold(folding, remainder, data);
    }
  }
  return instructions::update(remainder, data);
}

// CRC32 instructions where they handle the parameter set, else folding from its minimum_size bytes, else
// loop_sliced(). table: the crc_lut_sliced table or the first 8 slices of the crc_lut_braided one.
template <isa Isa, bool Reflected, class Register>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the polynomial and the remainder are both register values.
Register run_sliced(Register const *table, folding_constants const &folding, Register polynomial, Register remainder,
                    std::span<std::byte const> data) noexcept {
  if constexpr(Reflected && std::same_as<Register, std::uint32_t>) {
    if constexpr(crc32_kernel<Isa, crc32_reflected_polynomial>::available) {
      if(polynomial == crc32_reflected_polynomial) {
        return run_crc32_instructions<Isa, crc32_reflected_polynomial>(folding, remainder, data);
      }
    }
    if constexpr(crc32_kernel<Isa, crc32c_reflected_polynomial>::available) {
      if(polynomial == crc32c_reflected_polynomial) {
        return run_crc32_instructions<Isa, crc32c_reflected_polynomial>(folding, remainder, data);
      }
    }
  }
  if constexpr(folding_kernel<Isa>::available) {
    if constexpr(folding_kernel<Isa>::wide_minimum_size != std::numeric_limits<std::size_t>::max()) {
      if(data.size() >= folding_kernel<Isa>::wide_minimum_size) {
        return fold_long_message<Isa, Reflected>(folding, remainder, data);
      }
    }
    if(data.size() >= folding_kernel<Isa>::minimum_size) {
      return fold_message<Isa, Reflected>(folding, remainder, data);
    }
  }
  return loop_sliced<Reflected>(table, remainder, data);
}

// crc_lut_braided: braid_count interleaved word streams over whole blocks from braided_minimum_size bytes, the rest via run_sliced().
// Below this, slicing-by-8 is faster (x86-64).
inline constexpr std::size_t braided_minimum_size = 128;

// Consumes all whole blocks: each stream's register (zero but the first) folds into its next word via the braid slices;
// the last block's words are combined into one register with the slicing-by-8 slices.
template <bool Reflected, class Register>
Register loop_braided(Register const *table, Register remainder, std::span<std::byte const> &data) noexcept {
  constexpr unsigned word_bits = 64;
  constexpr std::size_t block_size = braid_count * word_size;
  using lanes = std::make_index_sequence<braid_count>;
  using word_bytes = std::make_index_sequence<word_size>;
  auto const align = [](promoted_register<Register> value) {
    return Reflected ? std::uint64_t{value} : std::uint64_t{value} << (word_bits - register_bits<Register>);
  };
  // One slice per byte: First == slice_count carries byte k over the block to the stream's next word, First == 0 folds
  // the word into the register with the slicing-by-8 slices.
  auto const fold_word = [&]<std::size_t First>(std::integral_constant<std::size_t, First>, std::uint64_t word) {
    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
      constexpr auto slice = [](std::size_t index) { return First == 0 ? slice_count - 1 - index : First + index; };
      return (promoted_register<Register>{table[(slice(Index) * slice_size) + ((word >> byte_shift<Reflected>(Index)) & byte_mask)]} ^ ...);
    }(word_bytes{});
  };
  std::array<promoted_register<Register>, braid_count> braids{remainder};
  for(std::size_t blocks = data.size() / block_size; blocks > 1; --blocks) {
    [&]<std::size_t... Lane>(std::index_sequence<Lane...>) {
      std::array<std::uint64_t, braid_count> const words{(load_word<Reflected>(data.subspan(Lane * word_size)) ^ align(braids[Lane]))...};
      ((braids[Lane] = fold_word(std::integral_constant<std::size_t, slice_count>{}, words[Lane])), ...);
    }(lanes{});
    data = data.subspan(block_size);
  }
  promoted_register<Register> combined = 0;
  for(std::size_t lane = 0; lane < braid_count; ++lane) {
    std::uint64_t const word = load_word<Reflected>(data.subspan(lane * word_size)) ^ align(braids[lane]) ^ align(combined);
    combined = fold_word(std::integral_constant<std::size_t, 0>{}, word);
  }
  data = data.subspan(block_size);
  return static_cast<Register>(combined);
}

// Presets with a folding kernel, and the CRC32 instruction cases, run only run_sliced().
template <isa Isa, bool Reflected, class Register>
Register run_braided(Register const *table, folding_constants const &folding, Register polynomial, Register remainder,
                     std::span<std::byte const> data) noexcept {
  if constexpr(!folding_kernel<Isa>::available) {
    if(data.size() >= braided_minimum_size && !uses_crc32_instructions<Isa, Reflected>(polynomial)) {
      remainder = loop_braided<Reflected>(table, remainder, data);
    }
  }
  return run_sliced<Isa, Reflected>(table, folding, polynomial, remainder, data);
}

} // namespace
} // namespace checksum::crc_detail

#include <checksum_private/target_end.hpp>

namespace checksum::crc_detail {

// The dispatch engines choose this preset exactly where its loop differs from generic's: it has target options or a kernel.
static_assert(own_loops<crc_lut_sliced, std::uint32_t>.contains(isa::CHECKSUM_PRESET) ==
              (!isa_detail::without_target_options(isa::CHECKSUM_PRESET) || folding_kernel<isa::CHECKSUM_PRESET>::available));

} // namespace checksum::crc_detail

#ifdef CHECKSUM_CRC_SLICED_ENTRY
namespace checksum::crc_detail {

// GCC fixes a function's target at its first declaration, which for update_loop() is in crc.hpp: it only forwards to
// run_sliced(), first declared inside the target region.
template <isa Isa, bool Reflected, class Register>
Register update_loop(lookup_table<Register, sliced_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept {
  return run_sliced<Isa, Reflected>(table.entries.data(), table.folding, table.form.polynomial, remainder, data);
}

template std::uint8_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint8_t, sliced_table_size> const &, std::uint8_t,
                                                              std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint16_t, sliced_table_size> const &, std::uint16_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint32_t, sliced_table_size> const &, std::uint32_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint64_t, sliced_table_size> const &, std::uint64_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint8_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint8_t, sliced_table_size> const &, std::uint8_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint16_t, sliced_table_size> const &, std::uint16_t,
                                                                std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint32_t, sliced_table_size> const &, std::uint32_t,
                                                                std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint64_t, sliced_table_size> const &, std::uint64_t,
                                                                std::span<std::byte const>) noexcept;

} // namespace checksum::crc_detail
#endif

#ifdef CHECKSUM_CRC_BRAIDED_ENTRY
namespace checksum::crc_detail {

// Forwards to run_braided(), first declared inside the target region, as update_loop() of crc_lut_sliced does.
template <isa Isa, bool Reflected, class Register>
Register update_loop(lookup_table<Register, braided_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept {
  return run_braided<Isa, Reflected>(table.entries.data(), table.folding, table.form.polynomial, remainder, data);
}

template std::uint8_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint8_t, braided_table_size> const &, std::uint8_t,
                                                              std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint16_t, braided_table_size> const &, std::uint16_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint32_t, braided_table_size> const &, std::uint32_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<isa::CHECKSUM_PRESET, true>(lookup_table<std::uint64_t, braided_table_size> const &, std::uint64_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint8_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint8_t, braided_table_size> const &, std::uint8_t,
                                                               std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint16_t, braided_table_size> const &, std::uint16_t,
                                                                std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint32_t, braided_table_size> const &, std::uint32_t,
                                                                std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<isa::CHECKSUM_PRESET, false>(lookup_table<std::uint64_t, braided_table_size> const &, std::uint64_t,
                                                                std::span<std::byte const>) noexcept;

} // namespace checksum::crc_detail
#endif

#endif // CHECKSUM_PRIVATE_CRC_TABLE_LOOPS_HPP
