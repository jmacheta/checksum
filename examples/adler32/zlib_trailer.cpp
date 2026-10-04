// Checks the Adler-32 trailer of a zlib stream (RFC 1950): the checksum of the uncompressed data, most significant byte first.

#include <checksum/adler32.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

namespace {

// The 4-byte big-endian number at data.
std::uint32_t load_big_endian(std::span<std::byte const, 4> data) {
  std::uint32_t value = 0;
  for(std::byte const byte : data) {
    value = (value << 8U) | std::to_integer<std::uint32_t>(byte);
  }
  return value;
}

} // namespace

int main() {
  // zlib header 78 01, one final stored block of 9 bytes (01, length 0009, its complement FFF6), "Wikipedia", Adler-32.
  constexpr std::array<std::uint8_t, 20> stream{0x78, 0x01, 0x01, 0x09, 0x00, 0xF6, 0xFF, 'W',  'i',  'k',
                                                'i',  'p',  'e',  'd',  'i',  'a',  0x11, 0xE6, 0x03, 0x98};
  auto const bytes = std::as_bytes(std::span(stream));
  auto const data = bytes.subspan(7, 9);
  std::uint32_t const stored = load_big_endian(bytes.last<4>());

  std::uint32_t const computed = checksum::adler32_compute(data);
  std::println("stored 0x{:08X}, computed 0x{:08X}: {}", stored, computed, stored == computed ? "intact" : "corrupted");
  return stored == computed && computed == 0x11E60398 ? 0 : 1;
}
