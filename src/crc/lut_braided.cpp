// crc_lut_braided loops: braid_count interleaved word streams over whole blocks from braided_minimum_size bytes, the
// rest via run_sliced(). Builds with a folding kernel, and the CRC32 instruction cases, run only run_sliced().

#include <checksum/crc.hpp>
#include <checksum_private/crc_table_loops.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace checksum::crc_detail {
namespace {

// Below this, slicing-by-8 is faster (x86-64).
constexpr std::size_t braided_minimum_size = 128;

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

template <bool Reflected, class Register>
Register run_braided(Register const *table, folding_constants const &folding, Register polynomial, Register remainder,
                     std::span<std::byte const> data) noexcept {
  if constexpr(!folding_available) {
    if(data.size() >= braided_minimum_size && !uses_crc32_instructions<Reflected>(polynomial)) {
      remainder = loop_braided<Reflected>(table, remainder, data);
    }
  }
  return run_sliced<Reflected>(table, folding, polynomial, remainder, data);
}

} // namespace

template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, braided_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept {
  return run_braided<Reflected>(table.entries.data(), table.folding, table.form.polynomial, remainder, data);
}

template std::uint8_t update_loop<true>(lookup_table<std::uint8_t, braided_table_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<true>(lookup_table<std::uint16_t, braided_table_size> const &, std::uint16_t, std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<true>(lookup_table<std::uint32_t, braided_table_size> const &, std::uint32_t, std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<true>(lookup_table<std::uint64_t, braided_table_size> const &, std::uint64_t, std::span<std::byte const>) noexcept;
template std::uint8_t update_loop<false>(lookup_table<std::uint8_t, braided_table_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<false>(lookup_table<std::uint16_t, braided_table_size> const &, std::uint16_t,
                                          std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<false>(lookup_table<std::uint32_t, braided_table_size> const &, std::uint32_t,
                                          std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<false>(lookup_table<std::uint64_t, braided_table_size> const &, std::uint64_t,
                                          std::span<std::byte const>) noexcept;

} // namespace checksum::crc_detail
