// Finds the distinct 4 KiB blocks of a buffer by their XXH3-128 hash, as a deduplicating store does.

#include <checksum/xxh3.hpp>

#include <cstddef>
#include <cstdint>
#include <print>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace std::literals;

// The hash is uniformly distributed, so its lower half is a good bucket index.
struct hash128_hasher {
  std::size_t operator()(checksum::xxh3_hash128 const &hash) const noexcept { return static_cast<std::size_t>(hash.low); }
};

// The XXH3-128 check value of "abc".
static_assert(checksum::xxh3_128_compute("abc"sv) == checksum::xxh3_hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});

int main() {
  constexpr std::size_t block_size = 4096;
  constexpr std::size_t block_count = 64;
  constexpr std::size_t pattern_count = 5;

  // A disk image whose blocks repeat five patterns.
  std::vector<std::byte> image(block_size * block_count);
  for(std::size_t index = 0; index < image.size(); ++index) {
    std::size_t const pattern = (index / block_size) % pattern_count;
    image[index] = static_cast<std::byte>(((index % block_size) * (pattern + 1)) ^ pattern);
  }

  // Block number of the first copy of each content.
  std::unordered_map<checksum::xxh3_hash128, std::size_t, hash128_hasher> first_copy;
  for(std::size_t block = 0; block < block_count; ++block) {
    std::span<std::byte const> const data = std::span(image).subspan(block * block_size, block_size);
    first_copy.try_emplace(checksum::xxh3_128_compute(data), block);
  }

  std::println("{} blocks, {} distinct, {} bytes saved", block_count, first_copy.size(), (block_count - first_copy.size()) * block_size);
  return first_copy.size() == pattern_count ? 0 : 1;
}
