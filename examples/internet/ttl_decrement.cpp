// A router decrements the TTL of an IPv4 header and updates its checksum without summing the header again (RFC 1624).
// It continues from the old checksum with the complemented old field, then with the new field.

#include <checksum/internet.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>

namespace {

// Bytes 8 and 9 (TTL and protocol) are the 16-bit field that changes; the checksum is bytes 10 and 11.
void decrement_ttl(std::span<std::byte, 20> header) {
  std::array<std::byte, 2> const old_field{~header[8], ~header[9]};
  auto const old_checksum = static_cast<std::uint16_t>((std::to_integer<unsigned>(header[10]) << 8U) | std::to_integer<unsigned>(header[11]));
  header[8] = static_cast<std::byte>(std::to_integer<unsigned>(header[8]) - 1U);

  constexpr checksum::internet_engine internet;
  auto const state = internet.update(internet.update(internet.resume(old_checksum), old_field), header.subspan<8, 2>());
  std::uint16_t const value = internet.finalize(state);
  header[10] = static_cast<std::byte>(value >> 8U);
  header[11] = static_cast<std::byte>(value & 0xFFU);
}

} // namespace

int main() {
  // TTL 64, UDP, 192.168.0.1 to 192.168.0.199, checksum 0xB861.
  std::array<std::byte, 20> header{std::byte{0x45}, std::byte{0x00}, std::byte{0x00}, std::byte{0x73}, std::byte{0x00},
                                   std::byte{0x00}, std::byte{0x40}, std::byte{0x00}, std::byte{0x40}, std::byte{0x11},
                                   std::byte{0xB8}, std::byte{0x61}, std::byte{0xC0}, std::byte{0xA8}, std::byte{0x00},
                                   std::byte{0x01}, std::byte{0xC0}, std::byte{0xA8}, std::byte{0x00}, std::byte{0xC7}};
  decrement_ttl(header);
  std::cout << "TTL " << std::to_integer<unsigned>(header[8]) << ", checksum 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
            << std::to_integer<unsigned>(header[10]) << std::setw(2) << std::to_integer<unsigned>(header[11]) << '\n';

  // The updated header still sums to 0, and its checksum 0xB961 is what summing the whole header gives.
  bool const valid = checksum::internet_compute(std::span<std::byte const>{header}) == 0;
  std::cout << "updated header valid: " << (valid ? "yes" : "no") << '\n';
  return valid && header[10] == std::byte{0xB9} && header[11] == std::byte{0x61} ? 0 : 1;
}
