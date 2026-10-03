// Builds an engine from parameters known only at run time, e.g. from a command line or a configuration file.

#include <checksum/crc.hpp>

#include <cstdint>
#include <cstdio> // stderr
#include <cstdlib>
#include <print>
#include <string_view>

using namespace std::literals;

int main(int argument_count, char **arguments) {
  if(argument_count != 4) {
    std::println(stderr, "usage: {} <polynomial in normal form, e.g. 0x1021> <width, e.g. 16> <initial value>", arguments[0]);
    return 1;
  }

  // create() takes the normal form and returns std::nullopt for an invalid polynomial.
  auto const polynomial =
      checksum::crc_polynomial::create(std::strtoull(arguments[1], nullptr, 0), static_cast<unsigned>(std::strtoul(arguments[2], nullptr, 0)));
  if(!polynomial) {
    std::println(stderr, "invalid polynomial: {} of width {}", arguments[1], arguments[2]);
    return 1;
  }
  std::uint64_t const initial_value = std::strtoull(arguments[3], nullptr, 0);

  // A 64-bit register fits every width. create() returns std::nullopt for width 0 or an initial value wider than the CRC.
  auto const engine = checksum::crc_engine<std::uint64_t, checksum::crc_lut_byte>::create({
      .polynomial = *polynomial,
      .initial_value = initial_value,
  });
  if(!engine) {
    std::println(stderr, "width 0 or an initial value wider than {} bits", polynomial->width());
    return 1;
  }

  std::println("width {}, check value (CRC of \"123456789\") = 0x{:X}", polynomial->width(), engine->compute("123456789"sv));
  return 0;
}
