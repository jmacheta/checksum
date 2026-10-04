// Hashes asset paths into 64-bit IDs at compile time, so that the program refers to assets by ID; the compiler rejects colliding
// IDs.

#include <checksum/xxh3.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies the XXH3-64 check value of "abc".
static_assert(checksum::xxh3_64_compute("abc"sv) == 0x78AF5F94892F3950);

namespace {

struct asset {
  std::string_view path;
  std::uint64_t id;
};

constexpr asset make_asset(std::string_view path) { return {.path = path, .id = checksum::xxh3_64_compute(path)}; }

// The table is a constant in read-only data, sorted by ID for a binary search.
constexpr auto assets = [] {
  std::array table{make_asset("textures/stone.png"), make_asset("textures/grass.png"), make_asset("sounds/step.wav"), make_asset("levels/intro.map")};
  std::ranges::sort(table, {}, &asset::id);
  return table;
}();
static_assert(std::ranges::adjacent_find(assets, {}, &asset::id) == assets.end(), "two paths have the same ID");

// The path of an ID, or an empty view for an unknown ID.
std::string_view find_path(std::uint64_t id) {
  auto const found = std::ranges::lower_bound(assets, id, {}, &asset::id);
  return found != assets.end() && found->id == id ? found->path : std::string_view{};
}

} // namespace

int main() {
  // A path read at run time, e.g. from a level file, hashes to the same ID.
  std::uint64_t const id = checksum::xxh3_64_compute("sounds/step.wav"sv);
  std::string_view const path = find_path(id);
  std::println("ID 0x{:016X} -> {}", id, path);
  return path == "sounds/step.wav" ? 0 : 1;
}
