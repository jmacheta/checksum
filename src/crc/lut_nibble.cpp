// Run-time loops of crc_lut_nibble, for every register type and bit order.

#include <checksum/crc.hpp>

#include <cstddef>
#include <span>

namespace checksum::crc_detail {

template <bool Reflected, class Register>
Register update_loop(lookup_table<Register, nibble_size> const &table, Register remainder, std::span<std::byte const> data) noexcept {
  return loop_nibble<Reflected>(table.entries.data(), remainder, data);
}

template std::uint8_t update_loop<true>(lookup_table<std::uint8_t, nibble_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<true>(lookup_table<std::uint16_t, nibble_size> const &, std::uint16_t, std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<true>(lookup_table<std::uint32_t, nibble_size> const &, std::uint32_t, std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<true>(lookup_table<std::uint64_t, nibble_size> const &, std::uint64_t, std::span<std::byte const>) noexcept;
template std::uint8_t update_loop<false>(lookup_table<std::uint8_t, nibble_size> const &, std::uint8_t, std::span<std::byte const>) noexcept;
template std::uint16_t update_loop<false>(lookup_table<std::uint16_t, nibble_size> const &, std::uint16_t, std::span<std::byte const>) noexcept;
template std::uint32_t update_loop<false>(lookup_table<std::uint32_t, nibble_size> const &, std::uint32_t, std::span<std::byte const>) noexcept;
template std::uint64_t update_loop<false>(lookup_table<std::uint64_t, nibble_size> const &, std::uint64_t, std::span<std::byte const>) noexcept;

} // namespace checksum::crc_detail
