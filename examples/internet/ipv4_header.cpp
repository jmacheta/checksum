// Fills in the checksum of an IPv4 header and verifies a received one.
// A header that holds its correct checksum sums to 0.

#include <checksum/internet.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

namespace {

// The checksum field is bytes 10 and 11, most significant byte first; it is zero while the checksum is computed.
void fill_checksum(std::span<std::byte, 20> header) {
  header[10] = std::byte{0};
  header[11] = std::byte{0};
  std::uint16_t const value = checksum::internet_compute(std::span<std::byte const>{header});
  header[10] = static_cast<std::byte>(value >> 8U);
  header[11] = static_cast<std::byte>(value & 0xFFU);
}

bool is_valid(std::span<std::byte const> header) { return checksum::internet_compute(header) == 0; }

} // namespace

int main() {
  // Version 4, total length 0x73, TTL 64, UDP, 192.168.0.1 to 192.168.0.199.
  std::array<std::byte, 20> header{std::byte{0x45}, std::byte{0x00}, std::byte{0x00}, std::byte{0x73}, std::byte{0x00},
                                   std::byte{0x00}, std::byte{0x40}, std::byte{0x00}, std::byte{0x40}, std::byte{0x11},
                                   std::byte{0x00}, std::byte{0x00}, std::byte{0xC0}, std::byte{0xA8}, std::byte{0x00},
                                   std::byte{0x01}, std::byte{0xC0}, std::byte{0xA8}, std::byte{0x00}, std::byte{0xC7}};
  fill_checksum(header);
  auto const value = static_cast<std::uint16_t>((std::to_integer<unsigned>(header[10]) << 8U) | std::to_integer<unsigned>(header[11]));
  std::println("IPv4 header checksum = 0x{:04X}", value);

  bool const intact = is_valid(header);
  header[15] ^= std::byte{0x01}; // a transmission error in the source address
  bool const corrupted = is_valid(header);
  std::println("intact header valid: {}, corrupted header valid: {}", intact ? "yes" : "no", corrupted ? "yes" : "no");
  return value == 0xB861 && intact && !corrupted ? 0 : 1;
}
