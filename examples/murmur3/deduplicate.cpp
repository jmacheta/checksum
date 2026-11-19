// Finds duplicate records by their MurmurHash3_x64_128 hash, folding each record from its fields with the streaming API.

#include <checksum/murmur3.hpp>

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>
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
  constexpr checksum::murmur3_engine<128> murmur3(0x5EED);
  auto state = murmur3.update(murmur3.initial(), value.name);
  state = murmur3.update(state, "\037"sv);
  state = murmur3.update(state, value.payload);
  return murmur3.finalize(state);
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
    std::cout << std::hex << std::setfill('0') << std::setw(16) << hash.high << std::setw(16) << hash.low << std::dec << "  " << value.name << ' '
              << value.payload << (is_new ? "" : "  (duplicate)") << '\n';
  }
  std::cout << seen.size() << " unique, " << duplicates << " duplicates\n";
  // The streamed hash equals the hash of the whole record in one call.
  bool const streaming_matches = record_hash(records[0]) == checksum::murmur3_compute<128>("sensor-1\037"
                                                                                           "21.5 C"sv,
                                                                                           0x5EED);
  return seen.size() == 3 && duplicates == 2 && streaming_matches ? 0 : 1;
}
