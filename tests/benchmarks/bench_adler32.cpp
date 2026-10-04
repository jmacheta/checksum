// Throughput of the Adler-32 checksum.

#include <checksum/adler32.hpp>

#include <benchmark/benchmark.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

void adler32(benchmark::State &state) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0) + state.range(1)));
  auto message = std::span<std::byte const>(data).subspan(static_cast<std::size_t>(state.range(1)));
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::adler32_compute(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// Sizes from IPv4 and UDP headers to a large buffer, including the kernel thresholds; the second argument starts the message 1
// byte after an aligned address.
BENCHMARK(adler32)->ArgsProduct({{20, 64, 96, 128, 160, 192, 256, 1500, 4096, std::int64_t{1} << 20}, {0, 1}});

} // namespace

BENCHMARK_MAIN();
