// A CRC engine for an instruction-set preset this build lacks is a static_assert, not a link error.
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/isa.hpp>

#include <cstdint>
#include <string_view>

using namespace std::literals;

// A preset of another architecture.
constexpr checksum::isa preset = checksum::isa_available(checksum::isa::x86_pclmul) ? checksum::isa::riscv_zbc : checksum::isa::x86_pclmul;

static_assert(!checksum::isa_available(preset));

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
std::uint32_t text_crc() {
  return checksum::crc_engine<std::uint32_t, checksum::crc_lut_sliced, preset>::create(checksum::crc32c)->compute("123456789"sv);
}
#else
std::uint32_t text_crc() { return checksum::crc_engine<std::uint32_t, checksum::crc_lut_sliced>::create(checksum::crc32c)->compute("123456789"sv); }
#endif
