// Computes the CRC-32 of a file in chunks, like the `crc32` command-line tool.

#include <checksum/crc_catalog.hpp>

#include <array>
#include <cstddef>
#include <cstdio> // stderr
#include <fstream>
#include <print>
#include <span>

int main(int argument_count, char **arguments) {
  if(argument_count != 2) {
    std::println(stderr, "usage: {} <file>", arguments[0]);
    return 1;
  }
  std::ifstream file(arguments[1], std::ios::binary);
  if(!file) {
    std::println(stderr, "cannot open {}", arguments[1]);
    return 1;
  }

  // The accumulator references the engine and keeps the running state across chunks.
  checksum::crc_accumulator accumulator{checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>};
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    accumulator.update(std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::println("{:08x}  {}", accumulator.value(), arguments[1]);
  return 0;
}
