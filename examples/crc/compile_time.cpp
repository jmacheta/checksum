// Computes CRCs at compile time: the engine, its table and the result are constants.

#include <checksum/crc.hpp>

#include <iomanip>
#include <iostream>
#include <string_view>

using namespace std::literals;

// A constant in read-only data: no code computes it at run time.
constexpr auto version_crc = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_byte>.compute("firmware 1.4.2"sv);

// The compiler verifies the CRC-32 check value.
static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

int main() {
  std::cout << "CRC-32 of \"firmware 1.4.2\" = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << version_crc << '\n';
  return 0;
}
