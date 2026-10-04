// Throughput of the built-in strategies, with zlib's crc32 and inline byte-table loops as baselines.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include <benchmark/benchmark.h>
#include <test_data.hpp>
#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio> // stderr
#include <print>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace checksum;

constexpr std::array<std::size_t, 6> sizes{16, 32, 64, 256, 4096, std::size_t{1} << 20};

std::vector<std::byte> const &input() {
  static std::vector<std::byte> const data = random_bytes(sizes.back());
  return data;
}

void run(benchmark::State &state, auto compute) {
  std::span<std::byte const> data = std::span(input()).first(static_cast<std::size_t>(state.range(0)));
  for(auto _ : state) {
    benchmark::DoNotOptimize(data);
    benchmark::DoNotOptimize(compute(data));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

template <crc_parameters Parameters, class Strategy> void register_engine(char const *strategy, char const *name) {
  auto *bench = benchmark::RegisterBenchmark(std::string(strategy) + "/" + name, [](benchmark::State &state) {
    run(state, [](auto data) { return crc_engine_for<Parameters, Strategy>.compute(data); });
  });
  for(std::size_t const size : sizes) {
    bench->Arg(static_cast<std::int64_t>(size));
  }
}

// Widths 8, 16, 32 and 64, each with reflected and unreflected input.
template <class Strategy> void register_strategy(char const *strategy) {
  register_engine<crc8_smbus, Strategy>(strategy, "crc8_smbus");
  register_engine<crc8_maxim_dow, Strategy>(strategy, "crc8_maxim_dow");
  register_engine<crc16_xmodem, Strategy>(strategy, "crc16_xmodem");
  register_engine<crc16_kermit, Strategy>(strategy, "crc16_kermit");
  register_engine<crc32_bzip2, Strategy>(strategy, "crc32_bzip2");
  register_engine<crc32_iso_hdlc, Strategy>(strategy, "crc32_iso_hdlc");
  register_engine<crc64_ecma_182, Strategy>(strategy, "crc64_ecma_182");
  register_engine<crc64_xz, Strategy>(strategy, "crc64_xz");
}

// zlib's crc32 computes CRC-32/ISO-HDLC.
std::uint32_t zlib_crc32(std::span<std::byte const> data) {
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
  auto const data = std::span<std::byte const>(input());
  if(zlib_crc32(data) != crc_engine_for<crc32_iso_hdlc, crc_lut_braided>.compute(data) ||
     inline_iso_hdlc(data) != crc_engine_for<crc32_iso_hdlc, crc_lut_byte>.compute(data) ||
     inline_xmodem(data) != crc_engine_for<crc16_xmodem, crc_lut_byte>.compute(data)) {
    std::println(stderr, "baseline mismatch");
    return 1;
  }

  register_strategy<crc_lut_none>("none");
  register_strategy<crc_lut_nibble>("nibble");
  register_strategy<crc_lut_byte>("byte");
  register_strategy<crc_lut_sliced>("sliced");
  register_strategy<crc_lut_braided>("braided");

  for(std::size_t const size : sizes) {
    benchmark::RegisterBenchmark("zlib/crc32_iso_hdlc", [](benchmark::State &state) {
      run(state, zlib_crc32);
    })->Arg(static_cast<std::int64_t>(size));
  }
  benchmark::RegisterBenchmark("inline_byte/crc32_iso_hdlc", [](benchmark::State &state) { run(state, inline_iso_hdlc); })->Arg(16);
  benchmark::RegisterBenchmark("inline_byte/crc16_xmodem", [](benchmark::State &state) { run(state, inline_xmodem); })->Arg(16);

  benchmark::Initialize(&argc, argv);
  if(benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
