// Computes the XXH64 hash of a file in chunks, in the output format of `xxh64sum`.

#include <checksum/xxhash.hpp>

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

  // The state is a plain value: each update returns it with one more chunk folded in.
  checksum::xxh64_state state;
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = checksum::xxhash_update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::println("{:016x}  {}", checksum::xxhash_finalize(state), arguments[1]);
  return 0;
}
