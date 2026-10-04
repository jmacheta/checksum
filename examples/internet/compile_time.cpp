// Builds an IPv4 header template with its checksum filled in at compile time, for a device that always sends the same header.

#include <checksum/internet.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies the check value of "123456789".
static_assert(checksum::internet_compute("123456789"sv) == 0xF62A);

namespace {

// Returns header with its checksum, bytes 10 and 11, filled in most significant byte first.
constexpr std::array<std::byte, 20> with_checksum(std::array<std::byte, 20> header) {
  header[10] = std::byte{0};
  header[11] = std::byte{0};
  std::uint16_t const value = checksum::internet_compute(header);
  header[10] = static_cast<std::byte>(value >> 8U);
  header[11] = static_cast<std::byte>(value & 0xFFU);
  return header;
}

// Version 4, total length 0x73, TTL 64, UDP, 192.168.0.1 to 192.168.0.199; a constant in read-only data.
constexpr std::array<std::byte, 20> header_template =
    with_checksum({std::byte{0x45}, std::byte{0x00}, std::byte{0x00}, std::byte{0x73}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
                   std::byte{0x00}, std::byte{0x40}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0}, std::byte{0xA8},
                   std::byte{0x00}, std::byte{0x01}, std::byte{0xC0}, std::byte{0xA8}, std::byte{0x00}, std::byte{0xC7}});
static_assert(header_template[10] == std::byte{0xB8} && header_template[11] == std::byte{0x61});

} // namespace

int main() {
  // A receiver sums the header with its checksum at run time and gets 0.
  bool const valid = checksum::internet_compute(header_template) == 0;
  std::println("IPv4 header checksum = 0x{:02X}{:02X}, header valid: {}", std::to_integer<unsigned>(header_template[10]),
               std::to_integer<unsigned>(header_template[11]), valid ? "yes" : "no");
  return valid ? 0 : 1;
}
