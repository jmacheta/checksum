// A Bloom filter of user names: one MurmurHash3_x64_128 hash per key gives all its bit positions.

#include <checksum/murmur3.hpp>

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

namespace {

class bloom_filter {
public:
  void insert(std::string_view key) {
    for(std::size_t index = 0; index < hash_count; ++index) {
      bits.set(position(key, index));
    }
  }

  // False means the key was never inserted; true means it probably was.
  [[nodiscard]] bool may_contain(std::string_view key) const {
    for(std::size_t index = 0; index < hash_count; ++index) {
      if(!bits.test(position(key, index))) {
        return false;
      }
    }
    return true;
  }

private:
  static constexpr std::size_t bit_count = 1024;
  static constexpr std::size_t hash_count = 5;

  // The halves h1 and h2 combine into hash_count positions as h1 + index * h2 (Kirsch and Mitzenmacher).
  static std::size_t position(std::string_view key, std::size_t index) {
    auto const [h1, h2] = checksum::murmur3_128_compute(key);
    return static_cast<std::size_t>((h1 + (index * h2)) % bit_count);
  }

  std::bitset<bit_count> bits;
};

} // namespace

int main() {
  bloom_filter users;
  for(std::string_view const name : {"alice"sv, "bob"sv, "carol"sv, "dave"sv}) {
    users.insert(name);
  }
  bool all_found = true;
  for(std::string_view const name : {"alice"sv, "bob"sv, "carol"sv, "dave"sv}) {
    all_found = all_found && users.may_contain(name);
  }
  for(std::string_view const name : {"alice"sv, "mallory"sv, "trent"sv}) {
    std::println("{:8} {}", name, users.may_contain(name) ? "probably present" : "absent");
  }
  // A filter never reports an inserted key as absent.
  return all_found ? 0 : 1;
}
