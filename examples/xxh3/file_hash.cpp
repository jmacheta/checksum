// Computes the XXH3-64 hash of a file in chunks and prints it with the file name.

#include <checksum/xxh3.hpp>

#include <array>
#include <cstddef>
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

  // The dispatch engine runs the best stripe kernel for this CPU, e.g. AVX2. The state is a 328-byte value passed in
  // and out of each update: large chunks keep that copy negligible.
  checksum::xxh3_dispatch_engine<64> const xxh3;
  auto state = xxh3.initial();
  std::array<char, std::size_t{64} * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = xxh3.update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::cout << "XXH3_" << std::hex << std::setfill('0') << std::setw(16) << xxh3.finalize(state) << "  " << arguments[1] << '\n';
  return 0;
}
