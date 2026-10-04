// Computes the Fletcher-32 of a file in chunks. A chunk may end inside a 16-bit block: the state keeps the unfinished
// block.

#include <checksum/fletcher.hpp>

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

  checksum::fletcher32_state state;
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = checksum::fletcher_update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::println("{:08x}  {}", checksum::fletcher_finalize(state), arguments[1]);
  return 0;
}
