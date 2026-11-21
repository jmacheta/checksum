// Checks settings loaded at run time against an Adler-32 computed at compile time.

#include <checksum/adler32.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

using namespace std::literals;

// The compiler verifies the example of the Wikipedia article "Adler-32".
static_assert(checksum::adler32_compute("Wikipedia"sv) == 0x11E60398);

constexpr std::string_view default_settings = "baud=115200\nparity=none\nstop_bits=1\n"sv;

// A constant in read-only data: no code computes it at run time.
constexpr std::uint32_t default_settings_checksum = checksum::adler32_compute(default_settings);

int main() {
  // The settings as loaded into RAM, e.g. after a reset to defaults.
  std::string const settings(default_settings);
  std::uint32_t const value = checksum::adler32_compute(settings);
  std::cout << "Adler-32 of the settings 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << value << ", expected 0x"
            << std::setw(8) << default_settings_checksum << '\n';
  return value == default_settings_checksum ? 0 : 1;
}
