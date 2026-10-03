// Defines a CRC that is not in the catalog, with a small nibble table suited to microcontrollers.

#include <checksum/crc.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>

using namespace checksum;

// A custom CRC is a crc_parameters aggregate; this one equals CRC-16/MODBUS.
inline constexpr crc_parameters sensor_crc{
    .polynomial = 0xC002, // Koopman notation of normal form 0x8005
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
};

// crc_lut_nibble: a 32-byte table instead of the 512-byte crc_lut_byte one.
constexpr auto const &sensor_engine = crc_engine_for<sensor_crc, crc_lut_nibble>;

int main() {
  constexpr std::array<std::byte, 4> reading{std::byte{0x01}, std::byte{0x04}, std::byte{0x02}, std::byte{0xFF}};
  std::uint16_t const crc = sensor_engine.compute(reading);
  std::println("CRC of the sensor reading = 0x{:04X}", crc);
  return 0;
}
