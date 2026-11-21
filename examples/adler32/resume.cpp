// Extends a stored Adler-32 when data is appended, without reading the old data again: the checksum is the whole state.

#include <checksum/adler32.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

using namespace std::literals;

int main() {
  // The checksum of the first part, e.g. kept next to a log file.
  std::uint32_t const stored = checksum::adler32_compute("12345"sv);

  // Later, more data is appended: continue from the state of the stored checksum.
  constexpr checksum::adler32_engine adler32;
  std::uint32_t const extended = adler32.finalize(adler32.update(adler32.resume(stored), "6789"sv));

  std::uint32_t const whole = checksum::adler32_compute("123456789"sv);
  std::cout << "extended 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << extended << ", whole 0x" << std::setw(8) << whole
            << '\n';
  return extended == whole && whole == 0x091E01DE ? 0 : 1;
}
