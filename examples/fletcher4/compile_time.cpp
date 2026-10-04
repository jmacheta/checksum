// Computes the fletcher4 checksum of a constant 4 KiB block at compile time, the expected value a run-time check compares
// with.

#include <checksum/fletcher4.hpp>

#include <array>
#include <cstddef>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies a value computed with fletcher_4_scalar_native of OpenZFS.
static_assert(checksum::fletcher4_compute("abcdefgh"sv) == checksum::fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});

namespace {

// A test pattern written to a disk, e.g. by a self-test.
constexpr std::array<std::byte, 4096> make_pattern() {
  std::array<std::byte, 4096> block{};
  for(std::size_t index = 0; index < block.size(); ++index) {
    block[index] = static_cast<std::byte>((index * 31) + 7);
  }
  return block;
}

// The checksum of the pattern is a constant in read-only data; the block itself is not stored.
constexpr checksum::fletcher4_value pattern_checksum = checksum::fletcher4_compute(make_pattern());
static_assert(pattern_checksum == checksum::fletcher4_value{0x1F9FE020400, 0x3F56A75807600, 0x54A3344A4A66000, 0x4E81CF973F7DBB00});

} // namespace

int main() {
  // The block as read back at run time.
  std::array<std::byte, 4096> const block = make_pattern();
  checksum::fletcher4_value const value = checksum::fletcher4_compute(block);
  std::println("{:x}:{:x}:{:x}:{:x}", value[0], value[1], value[2], value[3]);
  return value == pattern_checksum ? 0 : 1;
}
