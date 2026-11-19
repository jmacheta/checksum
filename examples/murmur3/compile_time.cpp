// Dispatches text commands with a switch on their MurmurHash3_x86_32 hash: the case labels are hashed at compile time.

#include <checksum/murmur3.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

using namespace std::literals;

// The compiler verifies a reference value of MurmurHash3_x86_32.
static_assert(checksum::murmur3_32_compute("Hello, world!"sv, 1234) == 0xFAF6CDB3);

namespace {

constexpr std::uint32_t key(std::string_view command) { return checksum::murmur3_32_compute(command); }

// Two commands with the same hash would be duplicate case labels, a compile error. Unknown text that collides with a label runs
// that command; compare the text too where this matters.
std::string_view execute(std::string_view command) {
  switch(key(command)) {
  case key("start"):
    return "starting";
  case key("stop"):
    return "stopping";
  case key("status"):
    return "running";
  default:
    return "unknown command";
  }
}

} // namespace

int main() {
  for(std::string_view const command : {"status"sv, "stop"sv, "reboot"sv}) {
    std::cout << std::left << std::setfill(' ') << std::setw(8) << command << " 0x" << std::right << std::hex << std::uppercase << std::setfill('0')
              << std::setw(8) << key(command) << " -> " << execute(command) << '\n';
  }
  return execute("stop") == "stopping" && execute("reboot") == "unknown command" ? 0 : 1;
}
