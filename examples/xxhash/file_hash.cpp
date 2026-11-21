// Computes the XXH64 hash of a file in chunks, in the output format of `xxh64sum`.

#include <checksum/xxhash.hpp>

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

  // The state is a plain value: each update returns it with one more chunk folded in.
  constexpr checksum::xxhash_engine<64> xxh64;
  auto state = xxh64.initial();
  std::array<char, std::size_t{64} * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = xxh64.update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::cout << std::hex << std::setfill('0') << std::setw(16) << xxh64.finalize(state) << "  " << arguments[1] << '\n';
  return 0;
}
