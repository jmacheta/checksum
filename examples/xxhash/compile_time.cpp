// Hashes message names at compile time into 32-bit IDs and dispatches on them with a switch.

#include <checksum/xxhash.hpp>

#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies the reference value of XXH32("abc").
static_assert(checksum::xxh32_compute("abc"sv) == 0x32D153FF);

// The seed keeps these IDs apart from hashes of the same names elsewhere.
constexpr std::uint32_t message_id(std::string_view name) { return checksum::xxh32_compute(name, 0x4D534731); }

constexpr std::uint32_t temperature_id = message_id("sensor/temperature");
constexpr std::uint32_t humidity_id = message_id("sensor/humidity");
static_assert(temperature_id != humidity_id);

namespace {

void handle(std::string_view name) {
  // A run-time hash of the same text matches the constant.
  switch(message_id(name)) {
  case temperature_id:
    std::println("{}: temperature handler", name);
    break;
  case humidity_id:
    std::println("{}: humidity handler", name);
    break;
  default:
    std::println("{}: unknown message", name);
    break;
  }
}

} // namespace

int main() {
  std::println("ID of sensor/temperature = 0x{:08X}", temperature_id);
  handle("sensor/temperature");
  handle("sensor/humidity");
  handle("sensor/pressure");
  return 0;
}
