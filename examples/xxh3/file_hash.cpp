// Computes the XXH3-64 hash of a file in chunks and prints it with the file name.

#include <checksum/xxh3.hpp>

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

  // The state is a 336-byte value passed in and out of each update: large chunks keep that copy negligible.
  checksum::xxh3_64_state state;
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = checksum::xxh3_update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::cout << "XXH3_" << std::hex << std::setfill('0') << std::setw(16) << checksum::xxh3_finalize(state) << "  " << arguments[1] << '\n';
  return 0;
}
