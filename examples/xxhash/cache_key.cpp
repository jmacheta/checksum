// Content-addressed cache: a build step is skipped when the XXH64 of its source and options is already known.

#include <checksum/xxhash.hpp>

#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>

using namespace std::literals;

namespace {

// The options seed the hash of the source, so the same source built with other options gets another key.
std::uint64_t cache_key(std::string_view source, std::string_view options) {
  return checksum::xxh64_compute(source, checksum::xxh64_compute(options));
}

} // namespace

// The compiler verifies XXH64("abc") of the reference implementation.
static_assert(checksum::xxh64_compute("abc"sv) == 0x44BC2CF5AD770999);

int main() {
  std::unordered_map<std::uint64_t, std::string> cache;
  // Returns whether the step had to run.
  auto const build = [&](std::string_view source, std::string_view options) {
    std::uint64_t const key = cache_key(source, options);
    auto const [entry, inserted] = cache.try_emplace(key, std::string(options) + " build of " + std::string(source));
    std::println("{:016x}  {}: {}", key, inserted ? "built" : "cached", entry->second);
    return inserted;
  };

  bool const first = build("int main() {}", "-O2");
  bool const other_options = build("int main() {}", "-O0");
  bool const repeated = build("int main() {}", "-O2");
  return first && other_options && !repeated ? 0 : 1;
}
