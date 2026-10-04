// Finds duplicate records by their MurmurHash3_x64_128 hash, folding each record from its fields with the streaming API.

#include <checksum/murmur3.hpp>

#include <array>
#include <cstddef>
#include <print>
#include <string_view>
#include <unordered_set>

using namespace std::literals;

namespace {

struct record {
  std::string_view name;
  std::string_view payload;
};

// The fields are folded one after another; a separator keeps {"ab", "c"} and {"a", "bc"} apart.
checksum::hash128 record_hash(record const &value) {
  checksum::murmur3_128_state state{.seed = 0x5EED};
  state = checksum::murmur3_update(state, value.name);
  state = checksum::murmur3_update(state, "\037"sv);
  state = checksum::murmur3_update(state, value.payload);
  return checksum::murmur3_finalize(state);
}

// The hash is uniformly distributed, so its lower half is a good bucket index.
struct hash128_hasher {
  std::size_t operator()(checksum::hash128 const &hash) const noexcept { return static_cast<std::size_t>(hash.low); }
};

} // namespace

int main() {
  constexpr std::array records{
      record{"sensor-1", "21.5 C"}, record{"sensor-2", "19.0 C"}, record{"sensor-1", "21.5 C"},
      record{"sensor-1", "21.6 C"}, record{"sensor-2", "19.0 C"},
  };
  std::unordered_set<checksum::hash128, hash128_hasher> seen;
  std::size_t duplicates = 0;
  for(record const &value : records) {
    auto const hash = record_hash(value);
    bool const is_new = seen.insert(hash).second;
    duplicates += is_new ? 0 : 1;
    std::println("{:016x}{:016x}  {} {}{}", hash.high, hash.low, value.name, value.payload, is_new ? "" : "  (duplicate)");
  }
  std::println("{} unique, {} duplicates", seen.size(), duplicates);
  // The streamed hash equals the hash of the whole record in one call.
  bool const streaming_matches = record_hash(records[0]) == checksum::murmur3_128_compute("sensor-1\03721.5 C"sv, 0x5EED);
  return seen.size() == 3 && duplicates == 2 && streaming_matches ? 0 : 1;
}
