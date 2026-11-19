// Throughput of the Adler-32 checksum, for every preset.

#include <checksum/adler32.hpp>
#include <checksum/isa.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <checksum::isa Isa> void adler32(benchmark::State &state) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0) + state.range(1)));
  auto message = std::span<std::byte const>(data).subspan(static_cast<std::size_t>(state.range(1)));
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::adler32_compute<Isa>(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// Sizes from IPv4 and UDP headers to a large buffer, including the kernel thresholds; the second argument starts the message 1
// byte after an aligned address.
void sizes(benchmark::Benchmark *benchmark) { benchmark->ArgsProduct({{20, 64, 96, 128, 160, 192, 256, 1500, 4096, std::int64_t{1} << 20}, {0, 1}}); }

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets("adler32", [](auto preset) { return adler32<decltype(preset)::value>; }, sizes);
  return isa_bench::run(argc, argv);
}
