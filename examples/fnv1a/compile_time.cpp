// Looks up settings by the FNV-1a 32 hash of their names: the table of hashes is built and checked for collisions at compile time.

#include <checksum/fnv1a.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

using namespace std::literals;

// The compiler verifies a value of the FNV test suite.
static_assert(checksum::fnv1a_compute<32>("foobar"sv) == 0xBF9CF968);

namespace {

constexpr std::array names{"baud_rate"sv, "parity"sv, "stop_bits"sv, "flow_control"sv};

constexpr std::array<std::uint32_t, names.size()> hashes = [] {
  std::array<std::uint32_t, names.size()> result{};
  std::ranges::transform(names, result.begin(), [](std::string_view name) { return checksum::fnv1a_compute<32>(name); });
  return result;
}();

static_assert(std::ranges::none_of(hashes, [](std::uint32_t hash) { return std::ranges::count(hashes, hash) > 1; }), "two names collide");

// The index of name in names; names that are not settings but collide with one are caught by comparing the text.
std::optional<std::size_t> find(std::string_view name) {
  auto const found = std::ranges::find(hashes, checksum::fnv1a_compute<32>(name));
  if(found == hashes.end() || names[static_cast<std::size_t>(found - hashes.begin())] != name) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - hashes.begin());
}

} // namespace

int main() {
  for(std::string_view const name : {"parity"sv, "flow_control"sv, "speed"sv}) {
    std::optional<std::size_t> const index = find(name);
    std::cout << name << ": " << (index ? "setting " + std::to_string(*index) : "unknown") << '\n';
  }
  return find("stop_bits") == 2 && !find("speed") ? 0 : 1;
}
