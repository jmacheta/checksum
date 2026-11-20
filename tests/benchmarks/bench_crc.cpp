// Throughput of the built-in strategies, crc_lut_sliced also for every preset and through the dispatch engine, with zlib's
// crc32 and inline byte-table loops as baselines. Benchmarks are named <strategy>[_<preset>]/<crc>/<size>.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/isa.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>
#include <test_data.hpp>
#include <zlib.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace checksum;

// Also 16 and 32 bytes, the short inputs of doc/performance.md.
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {16, 32}); }

// crc_engine_for<Parameters, Strategy, Isa>, or the dispatch engine of Parameters and Strategy if Dispatch.
template <crc_parameters Parameters, class Strategy, isa Isa, bool Dispatch> decltype(auto) engine() {
  if constexpr(Dispatch) {
    using dispatch_engine = crc_dispatch_engine<crc_register_for<Parameters.polynomial.width()>, Strategy>;
    return dispatch_engine(dispatch_engine::create(Parameters).value());
  } else {
    return (crc_engine_for<Parameters, Strategy, Isa>);
  }
}

template <crc_parameters Parameters, class Strategy, isa Isa, bool Dispatch> void register_engine(std::string const &strategy, char const *name) {
  sizes(benchmark::RegisterBenchmark(strategy + "/" + name, [](benchmark::State &state) {
    auto const &crc = engine<Parameters, Strategy, Isa, Dispatch>();
    isa_bench::measure(state, [&](std::span<std::byte const> data) { return crc.compute(data); });
  }));
}

// Widths 8, 16, 32 and 64, each with reflected and unreflected input.
template <class Strategy, isa Isa = isa::generic, bool Dispatch = false> void register_strategy(std::string const &strategy) {
  register_engine<crc8_smbus, Strategy, Isa, Dispatch>(strategy, "crc8_smbus");
  register_engine<crc8_maxim_dow, Strategy, Isa, Dispatch>(strategy, "crc8_maxim_dow");
  register_engine<crc16_xmodem, Strategy, Isa, Dispatch>(strategy, "crc16_xmodem");
  register_engine<crc16_kermit, Strategy, Isa, Dispatch>(strategy, "crc16_kermit");
  register_engine<crc32_bzip2, Strategy, Isa, Dispatch>(strategy, "crc32_bzip2");
  register_engine<crc32_iso_hdlc, Strategy, Isa, Dispatch>(strategy, "crc32_iso_hdlc");
  register_engine<crc32c, Strategy, Isa, Dispatch>(strategy, "crc32c");
  register_engine<crc64_ecma_182, Strategy, Isa, Dispatch>(strategy, "crc64_ecma_182");
  register_engine<crc64_xz, Strategy, Isa, Dispatch>(strategy, "crc64_xz");
}

// zlib's crc32 computes CRC-32/ISO-HDLC.
std::uint32_t zlib_crc32(std::span<std::byte const> data) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,misc-include-cleaner): zlib takes Bytef from zconf.h, through zlib.h.
  return static_cast<std::uint32_t>(crc32_z(0, reinterpret_cast<Bytef const *>(data.data()), data.size()));
}

// Call-overhead baselines: crc_lut_byte loops written inline, sharing only the table with the library.
constexpr auto iso_hdlc_table = *crc_lut_byte::make_table<std::uint32_t>(crc32_iso_hdlc);
constexpr auto xmodem_table = *crc_lut_byte::make_table<std::uint16_t>(crc16_xmodem);

std::uint32_t inline_iso_hdlc(std::span<std::byte const> data) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for(std::byte const byte : data) {
    crc = iso_hdlc_table.entries[(crc ^ std::to_integer<std::uint32_t>(byte)) & 0xFFU] ^ (crc >> 8U);
  }
  return ~crc;
}

std::uint16_t inline_xmodem(std::span<std::byte const> data) {
  unsigned crc = 0;
  for(std::byte const byte : data) {
    crc = xmodem_table.entries[((crc >> 8U) ^ std::to_integer<unsigned>(byte)) & 0xFFU] ^ ((crc << 8U) & 0xFFFFU);
  }
  return static_cast<std::uint16_t>(crc);
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::byte> const data = random_bytes(std::size_t{1} << 20U);
  if(zlib_crc32(data) != crc_engine_for<crc32_iso_hdlc, crc_lut_braided>.compute(data) ||
     inline_iso_hdlc(data) != crc_engine_for<crc32_iso_hdlc, crc_lut_byte>.compute(data) ||
     inline_xmodem(data) != crc_engine_for<crc16_xmodem, crc_lut_byte>.compute(data)) {
    std::cerr << "baseline mismatch\n";
    return 1;
  }

  register_strategy<crc_lut_none>("none");
  register_strategy<crc_lut_nibble>("nibble");
  register_strategy<crc_lut_byte>("byte");
  isa_bench::for_each_preset([]<isa Preset>(std::string const &suffix) { register_strategy<crc_lut_sliced, Preset>("sliced" + suffix); });
  register_strategy<crc_lut_sliced, isa::generic, true>("sliced_dispatch");
  register_strategy<crc_lut_braided>("braided");

  sizes(benchmark::RegisterBenchmark("zlib/crc32_iso_hdlc", [](benchmark::State &state) { isa_bench::measure(state, zlib_crc32); }));
  benchmark::RegisterBenchmark("inline_byte/crc32_iso_hdlc", [](benchmark::State &state) { isa_bench::measure(state, inline_iso_hdlc); })->Arg(16);
  benchmark::RegisterBenchmark("inline_byte/crc16_xmodem", [](benchmark::State &state) { isa_bench::measure(state, inline_xmodem); })->Arg(16);
  return isa_bench::run(argc, argv);
}
