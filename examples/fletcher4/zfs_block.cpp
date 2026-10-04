// Verifies a 4 KiB block against its stored fletcher4 checksum, as ZFS does when it reads a block, and detects a flipped bit.

#include <checksum/fletcher4.hpp>

#include <array>
#include <cstddef>
#include <print>

namespace {

// The checksum in the notation of zdb: the four sums in hexadecimal, separated by colons.
void print_checksum(char const *label, checksum::fletcher4_value const &value) {
  std::println("{} {:x}:{:x}:{:x}:{:x}", label, value[0], value[1], value[2], value[3]);
}

} // namespace

int main() {
  std::array<std::byte, 4096> block{};
  for(std::size_t index = 0; index < block.size(); ++index) {
    block[index] = static_cast<std::byte>((index * 31) + 7);
  }

  // The checksum the block pointer stores for this block.
  constexpr checksum::fletcher4_value stored{0x1F9FE020400, 0x3F56A75807600, 0x54A3344A4A66000, 0x4E81CF973F7DBB00};

  checksum::fletcher4_value const computed = checksum::fletcher4_compute(block);
  print_checksum("stored  ", stored);
  print_checksum("computed", computed);
  if(computed != stored) {
    std::println("checksum mismatch");
    return 1;
  }

  block[1000] ^= std::byte{0x10};
  checksum::fletcher4_value const damaged = checksum::fletcher4_compute(block);
  print_checksum("damaged ", damaged);
  if(damaged == stored) {
    std::println("bit flip not detected");
    return 1;
  }
  std::println("bit flip detected");
  return 0;
}
