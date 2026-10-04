// Computes the fletcher4 checksum of a file in chunks; the state carries an unfinished word over to the next chunk.

#include <checksum/fletcher4.hpp>

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

  checksum::fletcher4_state state;
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = checksum::fletcher4_update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  // A size that is not a multiple of 4 is padded with zero bytes.
  checksum::fletcher4_value const value = checksum::fletcher4_finalize(state);
  std::println("{:x}:{:x}:{:x}:{:x}  {}", value[0], value[1], value[2], value[3], arguments[1]);
  return 0;
}
