// Bare-metal check of detect_isa() and the dispatch engines. The build defines CHECKSUM_EXPECTED_ISA, the preset that its
// compiler flags promise, and CHECKSUM_EXPECTED_BUILT, the last preset it builds; the program exits with 0 if detect_isa()
// reports exactly the promised set and every engine gives the results of isa::generic.

#include <checksum/adler32.hpp>
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/fletcher.hpp>
#include <checksum/fletcher4.hpp>
#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <checksum/murmur3.hpp>
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>
#include <checksum_private/compiled_isa.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <type_traits>

namespace {

using namespace checksum;

constexpr isa_set expected_set{isa::CHECKSUM_EXPECTED_ISA};
constexpr isa_set expected_built{isa::CHECKSUM_EXPECTED_BUILT};

// The presets of this build that compiler_guarantees() reports: what detect_isa() returns without a feature query.
consteval isa_set guaranteed_presets() {
  isa_set result;
  for(isa const preset : isa_detail::built_presets) {
    if(isa_detail::compiler_guarantees(preset)) {
      result = result | preset;
    }
  }
  return result;
}

// Whether isa_available() accepts exactly generic and the presets of the line up to CHECKSUM_EXPECTED_BUILT.
consteval bool built_as_expected() {
  for(unsigned value = 0; value <= static_cast<unsigned>(isa::riscv_v); ++value) {
    if(isa_available(static_cast<isa>(value)) != expected_built.contains(static_cast<isa>(value))) {
      return false;
    }
  }
  return true;
}

static_assert(guaranteed_presets() == expected_set, "the compiler flags promise other presets than CHECKSUM_EXPECTED_ISA");
static_assert(built_as_expected(), "the build has other presets than CHECKSUM_EXPECTED_BUILT and those before it");

std::array<std::byte, 4100> buffer{};
int failures = 0;

void check(bool equal, char const *what, isa preset, std::size_t size) {
  if(!equal) {
    ++failures;
    std::printf("FAIL %s, preset %u, size %u\n", what, static_cast<unsigned>(preset), static_cast<unsigned>(size));
  }
}

// One CRC dispatch engine made for cpu against the generic crc_lut_byte engine.
template <crc_parameters Parameters, class Strategy> void check_crc(isa_set cpu, std::span<std::byte const> data, char const *what) {
  auto const &reference = crc_engine_for<Parameters, crc_lut_byte>;
  using register_type = std::remove_cvref_t<decltype(reference)>::state_type;
  auto const engine = crc_dispatch_engine<register_type, Strategy>::create(Parameters, cpu);
  check(engine.has_value() && engine->compute(data) == reference.compute(data), what, engine ? engine->preset() : isa::generic, data.size());
}

// Every dispatch engine made for cpu against the generic templates, for lengths around the kernels' blocks and an unaligned start.
void check_engines(isa_set cpu) {
  internet_dispatch_engine const internet(cpu);
  adler32_dispatch_engine const adler32(cpu);
  fletcher_dispatch_engine<16> const fletcher16(cpu);
  fletcher_dispatch_engine<32> const fletcher32(cpu);
  fletcher_dispatch_engine<64> const fletcher64(cpu);
  fletcher4_dispatch_engine const fletcher4(cpu);
  xxh3_dispatch_engine<64> const xxh3_64(5, cpu);
  xxh3_dispatch_engine<128> const xxh3_128(0, cpu);
  xxhash_dispatch_engine<32> const xxh32(3, cpu);
  xxhash_dispatch_engine<64> const xxh64(3, cpu);
  murmur3_dispatch_engine<32> const murmur3_32(2, cpu);
  murmur3_dispatch_engine<128> const murmur3_128(2, cpu);
  isa const preset = internet.preset();
  check(preset == isa_detail::select(cpu) && cpu.contains(preset), "preset", preset, 0);
  for(std::size_t const size : {0UZ, 1UZ, 3UZ, 15UZ, 16UZ, 63UZ, 64UZ, 255UZ, 1500UZ, 4097UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ}) {
      auto const data = std::span<std::byte const>(buffer).subspan(offset, size);
      check(internet.compute(data) == internet_compute(data), "internet", preset, size);
      check(adler32.compute(data) == adler32_compute(data), "adler32", preset, size);
      check(fletcher16.compute(data) == fletcher_compute<16>(data), "fletcher16", preset, size);
      check(fletcher32.compute(data) == fletcher_compute<32>(data), "fletcher32", preset, size);
      check(fletcher64.compute(data) == fletcher_compute<64>(data), "fletcher64", preset, size);
      check(fletcher4.compute(data) == fletcher4_compute(data), "fletcher4", preset, size);
      check(xxh3_64.compute(data) == xxh3_compute<64>(data, 5), "xxh3_64", preset, size);
      check(xxh3_128.compute(data) == xxh3_compute<128>(data), "xxh3_128", preset, size);
      check(xxh32.compute(data) == xxhash_compute<32>(data, 3), "xxh32", preset, size);
      check(xxh64.compute(data) == xxhash_compute<64>(data, 3), "xxh64", preset, size);
      check(murmur3_32.compute(data) == murmur3_compute<32>(data, 2), "murmur3_32", preset, size);
      check(murmur3_128.compute(data) == murmur3_compute<128>(data, 2), "murmur3_128", preset, size);
      check_crc<crc32_iso_hdlc, crc_lut_sliced>(cpu, data, "crc32 sliced");
      check_crc<crc32c, crc_lut_braided>(cpu, data, "crc32c braided");
      check_crc<crc16_xmodem, crc_lut_sliced>(cpu, data, "crc16 sliced");
      check_crc<crc64_xz, crc_lut_braided>(cpu, data, "crc64 braided");
      check_crc<crc8_smbus, crc_lut_byte>(cpu, data, "crc8 byte");
    }
  }
}

} // namespace

int main() {
  for(std::size_t index = 0; index < buffer.size(); ++index) {
    buffer[index] = static_cast<std::byte>((index * 131U) + (index >> 7U) + 7U);
  }
  isa_set const detected = detect_isa();
  check(detected == expected_set, "detect_isa", isa::CHECKSUM_EXPECTED_ISA, 0);
  check(internet_dispatch_engine{}.preset() == isa_detail::select(detected), "default engine", internet_dispatch_engine{}.preset(), 0);
  check_engines(detected);
  for(isa const preset : isa_detail::built_presets) {
    if(detected.contains(preset)) {
      check_engines(preset);
    }
  }
  std::printf("%s: %d failures\n", failures == 0 ? "PASS" : "FAIL", failures);
  return failures == 0 ? 0 : 1;
}
