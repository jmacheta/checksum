// Naming an instruction-set preset this build lacks is a static_assert, not a link error.
#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include <cstdint>
#include <string_view>

using namespace std::literals;

// A preset of another architecture.
#if defined(__x86_64__)
constexpr checksum::isa preset = checksum::isa::riscv_v;
#else
constexpr checksum::isa preset = checksum::isa::x86_avx2;
#endif

static_assert(!checksum::isa_available(preset));

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
std::uint16_t text_checksum() { return checksum::internet_compute<preset>("123456789"sv); }
#else
std::uint16_t text_checksum() { return checksum::internet_compute("123456789"sv); }
#endif
