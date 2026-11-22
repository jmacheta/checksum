// Computes the checksum of a UDP datagram over IPv4 from its pieces: the pseudo-header, the UDP header and the payload.
// The pieces are never copied into one buffer, and the payload may have an odd length.

#include <checksum/internet.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

using namespace std::literals;

namespace {

constexpr std::array<std::byte, 4> source{std::byte{192}, std::byte{168}, std::byte{0}, std::byte{1}};
constexpr std::array<std::byte, 4> destination{std::byte{192}, std::byte{168}, std::byte{0}, std::byte{199}};
constexpr std::uint16_t protocol_udp = 17;
constexpr std::uint16_t source_port = 1234;
constexpr std::uint16_t destination_port = 53;
constexpr std::string_view payload = "hello, checksum"sv;
constexpr auto udp_length = static_cast<std::uint16_t>(8 + payload.size());

constexpr std::array<std::byte, 2> big_endian(std::uint16_t value) {
  return {static_cast<std::byte>(value >> 8U), static_cast<std::byte>(value & 0xFFU)};
}

// The checksum over the pseudo-header, the UDP header with checksum_field and the payload.
std::uint16_t udp_checksum(std::uint16_t checksum_field) {
  // Pseudo-header: source, destination, a zero byte and the protocol, the UDP length.
  constexpr checksum::internet_engine internet;
  auto state = internet.initial();
  state = internet.update(state, source);
  state = internet.update(state, destination);
  state = internet.update(state, big_endian(protocol_udp));
  state = internet.update(state, big_endian(udp_length));
  // UDP header: ports, length, checksum.
  state = internet.update(state, big_endian(source_port));
  state = internet.update(state, big_endian(destination_port));
  state = internet.update(state, big_endian(udp_length));
  state = internet.update(state, big_endian(checksum_field));
  state = internet.update(state, payload);
  return internet.finalize(state);
}

} // namespace

int main() {
  // The sender sums with a zero checksum field. UDP sends a computed 0 as 0xFFFF, because 0 means "no checksum".
  std::uint16_t value = udp_checksum(0);
  if(value == 0) {
    value = 0xFFFF;
  }
  std::cout << "UDP checksum = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << value << '\n';

  // The receiver sums the datagram as received and expects 0.
  bool const valid = udp_checksum(value) == 0;
  std::cout << "received datagram valid: " << (valid ? "yes" : "no") << '\n';
  return value == 0x67F8 && valid ? 0 : 1;
}
