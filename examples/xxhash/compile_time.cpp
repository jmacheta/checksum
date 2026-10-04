// Hashes message names at compile time into 32-bit IDs and dispatches on them with a switch.

#include <checksum/xxhash.hpp>

#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies the reference value of XXH32("abc").
static_assert(checksum::xxh32_compute("abc"sv) == 0x32D153FF);

namespace {

// The seed keeps these IDs apart from hashes of the same names elsewhere.
constexpr std::uint32_t message_id(std::string_view name) { return checksum::xxh32_compute(name, 0x4D534731); }

constexpr std::uint32_t temperature_id = message_id("sensor/temperature");
constexpr std::uint32_t humidity_id = message_id("sensor/humidity");
static_assert(temperature_id != humidity_id);

// The handler of a message; a run-time hash of the same text matches the constant.
std::string_view handler(std::string_view name) {
  switch(message_id(name)) {
  case temperature_id:
    return "temperature handler";
  case humidity_id:
    return "humidity handler";
  default:
    return "unknown message";
  }
}

} // namespace

int main() {
  for(std::string_view const name : {"sensor/temperature"sv, "sensor/humidity"sv, "sensor/pressure"sv}) {
    std::println("{:18} 0x{:08X} -> {}", name, message_id(name), handler(name));
  }
  return handler("sensor/humidity") == "humidity handler" && handler("sensor/pressure") == "unknown message" ? 0 : 1;
}
