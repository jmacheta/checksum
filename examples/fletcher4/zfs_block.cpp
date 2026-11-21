// Verifies a 4 KiB block against its stored fletcher4 checksum, as ZFS does on every read, and detects a flipped bit.

#include <checksum/fletcher4.hpp>

#include <array>
#include <cstddef>
#include <iostream>

namespace {

// The checksum in the notation of zdb: the four sums in hexadecimal, separated by colons.
void print_checksum(char const *label, checksum::fletcher4_value const &value) {
  std::cout << label << ' ' << std::hex << value[0] << ':' << value[1] << ':' << value[2] << ':' << value[3] << std::dec << '\n';
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
    std::cout << "checksum mismatch\n";
    return 1;
  }

  block[1000] ^= std::byte{0x10};
  checksum::fletcher4_value const damaged = checksum::fletcher4_compute(block);
  print_checksum("damaged ", damaged);
  if(damaged == stored) {
    std::cout << "bit flip not detected\n";
    return 1;
  }
  std::cout << "bit flip detected\n";
  return 0;
}
