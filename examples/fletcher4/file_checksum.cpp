// Computes the fletcher4 checksum of a file in chunks; the state carries an unfinished word over to the next chunk.

#include <checksum/fletcher4.hpp>

#include <array>
#include <cstddef>
#include <fstream>
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

  constexpr checksum::fletcher4_engine fletcher4;
  auto state = fletcher4.initial();
  std::array<char, std::size_t{64} * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = fletcher4.update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  // A size that is not a multiple of 4 is padded with zero bytes.
  checksum::fletcher4_value const value = fletcher4.finalize(state);
  std::cout << std::hex << value[0] << ':' << value[1] << ':' << value[2] << ':' << value[3] << "  " << arguments[1] << '\n';
  return 0;
}
