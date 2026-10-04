// Computes the Adler-32 of built-in default settings at compile time; at run time a copy of the settings is checked against
// that constant.

#include <checksum/adler32.hpp>

#include <cstdint>
#include <print>
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
  std::println("Adler-32 of the settings 0x{:08X}, expected 0x{:08X}", value, default_settings_checksum);
  return value == default_settings_checksum ? 0 : 1;
}
