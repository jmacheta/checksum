// Extends a stored Adler-32 when data is appended, without reading the old data again: the checksum is the whole state.

#include <checksum/adler32.hpp>

#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

int main() {
  // The checksum of the first part, e.g. kept next to a log file.
  std::uint32_t const stored = checksum::adler32_compute("12345"sv);

  // Later, more data is appended: resume from the stored value.
  checksum::adler32_state state{.sum1 = static_cast<std::uint16_t>(stored & 0xFFFFU), .sum2 = static_cast<std::uint16_t>(stored >> 16U)};
  state = checksum::adler32_update(state, "6789"sv);
  std::uint32_t const extended = checksum::adler32_finalize(state);

  std::uint32_t const whole = checksum::adler32_compute("123456789"sv);
  std::println("resumed 0x{:08X}, whole 0x{:08X}", extended, whole);
  return extended == whole && whole == 0x091E01DE ? 0 : 1;
}
