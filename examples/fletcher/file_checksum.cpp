// Computes the Fletcher-32 of a file in chunks. A chunk may end inside a 16-bit block: the state keeps the unfinished
// block.

#include <checksum/fletcher.hpp>

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

  constexpr checksum::fletcher_engine<32> fletcher32;
  auto state = fletcher32.initial();
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = fletcher32.update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::cout << std::hex << std::setfill('0') << std::setw(8) << fletcher32.finalize(state) << "  " << arguments[1] << '\n';
  return 0;
}
