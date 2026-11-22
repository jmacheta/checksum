// Checks a firmware image against the SHA-256 digest recorded at build time, reading the image in pieces as a bootloader
// would read flash.

#include <checksum/engine.hpp>
#include <checksum/sha.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

using namespace std::literals;
using checksum::sha_variant;

// The compiler verifies the FIPS 180-4 example.
static_assert(checksum::sha_compute<sha_variant::sha256>("abc"sv) ==
              std::array<std::uint8_t, 32>{0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
                                           0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD});

namespace {

// A stand-in for an image in flash.
constexpr std::array<std::byte, 3000> image = [] {
  std::array<std::byte, 3000> result{};
  for(std::size_t index = 0; index < result.size(); ++index) {
    result[index] = static_cast<std::byte>((index * 7) + 3);
  }
  return result;
}();

// The digest that the build would store next to the image: here computed by the compiler.
constexpr auto expected = checksum::sha_compute<sha_variant::sha256>(image);

bool image_valid(std::span<std::byte const> flash) {
  constexpr checksum::sha_engine<sha_variant::sha256> sha256;
  checksum::accumulator running{sha256};
  constexpr std::size_t page_size = 256;
  for(std::size_t offset = 0; offset < flash.size(); offset += page_size) {
    running.update(flash.subspan(offset, std::min(page_size, flash.size() - offset)));
  }
  return running.value() == expected;
}

} // namespace

int main() {
  std::array<std::byte, image.size()> corrupted = image;
  corrupted[1234] ^= std::byte{1};
  std::cout << "image: " << (image_valid(image) ? "valid" : "corrupted") << '\n';
  std::cout << "with one flipped bit: " << (image_valid(corrupted) ? "valid" : "corrupted") << '\n';
  return image_valid(image) && !image_valid(corrupted) ? 0 : 1;
}
