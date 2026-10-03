// Computes CRCs at compile time: the engine, its table and the result are constants.

#include <checksum/crc_catalog.hpp>

#include <print>
#include <string_view>

using namespace std::literals;

// crc_engine_for<Parameters, Strategy> is a constexpr engine; compute() runs in a constant expression.
constexpr auto version_crc = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_byte>.compute("firmware 1.4.2"sv);

// The compiler verifies the CRC-32 check value.
static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

int main() {
  std::println("CRC-32 of \"firmware 1.4.2\" = 0x{:08X}", version_crc);
  return 0;
}
