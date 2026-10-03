// crc_lut_sliced loops: slicing-by-8, or the folding kernel / CRC32 instructions where the compiler flags enable them
// (see run_sliced()).

#include <checksum/crc.hpp>
#include <checksum_private/crc_table_loops.hpp>

#include <span>

namespace checksum::crc_detail {

template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, sliced_table_size> const &table, Register remainder, std::span<std::byte const> data) noexcept {
  return run_sliced<Reflected>(table.entries.data(), table.folding, table.form.polynomial, remainder, data);
}

template std::uint8_t update_loop<true>(lookup_table<std::uint8_t, sliced_table_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<true>(lookup_table<std::uint16_t, sliced_table_size> const &, std::uint16_t, std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<true>(lookup_table<std::uint32_t, sliced_table_size> const &, std::uint32_t, std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<true>(lookup_table<std::uint64_t, sliced_table_size> const &, std::uint64_t, std::span<std::byte const>) noexcept;
template std::uint8_t update_loop<false>(lookup_table<std::uint8_t, sliced_table_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<false>(lookup_table<std::uint16_t, sliced_table_size> const &, std::uint16_t, std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<false>(lookup_table<std::uint32_t, sliced_table_size> const &, std::uint32_t, std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<false>(lookup_table<std::uint64_t, sliced_table_size> const &, std::uint64_t, std::span<std::byte const>) noexcept;

} // namespace checksum::crc_detail
