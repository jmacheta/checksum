// Computes the CRC-32 of a file in chunks, like the `crc32` command-line tool.

#include <checksum/crc.hpp>

#include <array>
#include <cstddef>
#include <cstdio> // stderr
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>

int main(int argument_count, char **arguments) {
  if(argument_count != 2) {
    std::cerr << "usage: " << arguments[0] << " <file>\n";
    return 1;
  }
  std::ifstream file(arguments[1], std::ios::binary);
  if(!file) {
    std::cerr << "cannot open " << arguments[1] << '\n';
    return 1;
  }

  // The accumulator references the engine and keeps the running state across chunks.
  checksum::accumulator crc{checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>};
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    crc.update(std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::cout << std::hex << std::setfill('0') << std::setw(8) << crc.value() << "  " << arguments[1] << '\n';
  return 0;
}
