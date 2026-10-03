// Appends a CRC-16/MODBUS to a frame and checks a received frame.
// The protocol defines the CRC byte order on the wire: MODBUS sends the low byte first.

#include <checksum/crc_catalog.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

namespace {

// One constexpr engine for both sides, with a 512-byte table.
constexpr auto const &modbus = checksum::crc_engine_for<checksum::crc16_modbus, checksum::crc_lut_byte>;

// frame is the message followed by two bytes that receive its CRC, low byte first.
void append_crc(std::span<std::byte> frame) {
  std::uint16_t const crc = modbus.compute(std::span<std::byte const>{frame}.first(frame.size() - 2));
  frame[frame.size() - 2] = static_cast<std::byte>(crc & 0xFFU);
  frame[frame.size() - 1] = static_cast<std::byte>(crc >> 8U);
}

// Recomputes the CRC of the message and compares it with the last two bytes.
bool is_valid(std::span<std::byte const> frame) {
  if(frame.size() < 2) {
    return false;
  }
  auto const received =
      static_cast<std::uint16_t>(std::to_integer<unsigned>(frame[frame.size() - 2]) | (std::to_integer<unsigned>(frame[frame.size() - 1]) << 8U));
  return modbus.compute(frame.first(frame.size() - 2)) == received;
}

} // namespace

int main() {
  // Read holding registers: slave 0x01, function 0x03, start 0x0000, count 0x000A, then two CRC bytes.
  std::array<std::byte, 8> frame{std::byte{0x01}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00},
                                 std::byte{0x00}, std::byte{0x0A}, std::byte{0x00}, std::byte{0x00}};
  append_crc(frame);
  std::println("CRC bytes on the wire: {:02X} {:02X}", std::to_integer<unsigned>(frame[6]), std::to_integer<unsigned>(frame[7]));

  bool const intact = is_valid(frame);
  frame[3] ^= std::byte{0x01}; // a transmission error
  bool const corrupted = is_valid(frame);
  std::println("intact frame valid: {}, corrupted frame valid: {}", intact ? "yes" : "no", corrupted ? "yes" : "no");
  return intact && !corrupted ? 0 : 1;
}
