// Computes Fletcher checksums at compile time: the results are constants.

#include <checksum/fletcher.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

using namespace std::literals;

// The compiler verifies the examples of the Wikipedia article "Fletcher's checksum".
static_assert(checksum::fletcher_compute<16>("abcde"sv) == 0xC8F0);
static_assert(checksum::fletcher_compute<32>("abcde"sv) == 0xF04FC729);
static_assert(checksum::fletcher_compute<64>("abcde"sv) == 0xC8C6C527646362C6);

// A constant in read-only data: no code computes it at run time.
constexpr std::uint32_t version_checksum = checksum::fletcher_compute<32>("firmware 1.4.2"sv);

int main() {
  // The same text at run time gives the same checksum.
  std::string const version = "firmware 1.4.2";
  std::uint32_t const value = checksum::fletcher_compute<32>(version);
  std::cout << "Fletcher-32 of \"" << version << "\" 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << value
            << ", expected 0x" << std::setw(8) << version_checksum << '\n';
  return value == version_checksum ? 0 : 1;
}
