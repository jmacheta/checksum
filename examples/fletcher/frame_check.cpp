// Appends a Fletcher-16 to a serial frame and checks a received frame.
// The checksum is a number; this protocol sends it most significant byte first, sum2 then sum1.

#include <checksum/fletcher.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

namespace {

// frame is the message followed by two bytes that receive its checksum.
void append_checksum(std::span<std::byte> frame) {
  std::uint16_t const value = checksum::fletcher16_compute(std::span<std::byte const>{frame}.first(frame.size() - 2));
  frame[frame.size() - 2] = static_cast<std::byte>(value >> 8U);
  frame[frame.size() - 1] = static_cast<std::byte>(value & 0xFFU);
}

// Recomputes the checksum of the message and compares it with the last two bytes.
bool is_valid(std::span<std::byte const> frame) {
  if(frame.size() < 2) {
    return false;
  }
  auto const received =
      static_cast<std::uint16_t>((std::to_integer<unsigned>(frame[frame.size() - 2]) << 8U) | std::to_integer<unsigned>(frame[frame.size() - 1]));
  return checksum::fletcher16_compute(frame.first(frame.size() - 2)) == received;
}

} // namespace

int main() {
  // The message "abcde", whose Fletcher-16 is 0xC8F0, then two checksum bytes.
  std::array<std::byte, 7> frame{std::byte{'a'}, std::byte{'b'}, std::byte{'c'}, std::byte{'d'}, std::byte{'e'}, std::byte{0}, std::byte{0}};
  append_checksum(frame);
  std::println("checksum bytes on the wire: {:02X} {:02X}", std::to_integer<unsigned>(frame[5]), std::to_integer<unsigned>(frame[6]));

  bool const expected = frame[5] == std::byte{0xC8} && frame[6] == std::byte{0xF0};
  bool const intact = is_valid(frame);
  frame[2] ^= std::byte{0x01}; // a transmission error
  bool const corrupted = is_valid(frame);
  std::println("intact frame valid: {}, corrupted frame valid: {}", intact ? "yes" : "no", corrupted ? "yes" : "no");
  return expected && intact && !corrupted ? 0 : 1;
}
