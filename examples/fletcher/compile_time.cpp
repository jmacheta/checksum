// Computes Fletcher checksums at compile time: the results are constants.

#include <checksum/fletcher.hpp>

#include <cstdint>
#include <print>
#include <string_view>

using namespace std::literals;

// The compiler verifies the examples of the Wikipedia article "Fletcher's checksum".
static_assert(checksum::fletcher16_compute("abcde"sv) == 0xC8F0);
static_assert(checksum::fletcher32_compute("abcde"sv) == 0xF04FC729);
static_assert(checksum::fletcher64_compute("abcde"sv) == 0xC8C6C527646362C6);

constexpr std::uint32_t version_checksum = checksum::fletcher32_compute("firmware 1.4.2"sv);

int main() {
  std::println("Fletcher-32 of \"firmware 1.4.2\" = 0x{:08X}", version_checksum);
  return 0;
}
