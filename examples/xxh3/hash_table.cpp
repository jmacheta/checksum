// A hash table keyed by strings, hashed with seeded XXH3-64.

#include <checksum/xxh3.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>

using namespace std::literals;

// Hashes keys with a seed chosen per table: tables with different seeds place the same keys differently. The seed does not protect
// against keys crafted to collide.
struct seeded_hasher {
  using is_transparent = void; // Lets find() take a std::string_view without building a std::string.
  std::uint64_t seed = 0;
  std::size_t operator()(std::string_view key) const noexcept { return static_cast<std::size_t>(checksum::xxh3_64_compute(key, seed)); }
};

// The XXH3-64 check value of "abc", computed at compile time.
static_assert(checksum::xxh3_64_compute("abc"sv) == 0x78AF5F94892F3950);

int main() {
  std::unordered_map<std::string, int, seeded_hasher, std::equal_to<>> port_by_service(16, seeded_hasher{.seed = 0x9E3779B97F4A7C15});
  port_by_service.emplace("ssh", 22);
  port_by_service.emplace("http", 80);
  port_by_service.emplace("https", 443);
  port_by_service.emplace("mqtt", 1883);

  auto const found = port_by_service.find("https"sv);
  if(found == port_by_service.end() || found->second != 443) {
    return 1;
  }
  std::println("https -> {}, hash 0x{:016X}", found->second, checksum::xxh3_64_compute("https"sv, port_by_service.hash_function().seed));
  return 0;
}
