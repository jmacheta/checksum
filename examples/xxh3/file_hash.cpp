// Computes the XXH3-64 hash of a file in chunks, like `xxhsum -H3`.

#include <checksum/xxh3.hpp>

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

  // The state is a 336-byte value passed in and out of each update: large chunks keep that copy negligible.
  checksum::xxh3_64_state state{};
  std::array<char, 64 * 1024> buffer{};
  while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    state = checksum::xxh3_update(state, std::span{buffer}.first(static_cast<std::size_t>(file.gcount())));
  }

  std::println("XXH3_{:016x}  {}", checksum::xxh3_finalize(state), arguments[1]);
  return 0;
}
