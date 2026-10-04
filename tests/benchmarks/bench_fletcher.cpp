// Throughput of the Fletcher checksums.

#include <checksum/fletcher.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <unsigned Width> void fletcher(benchmark::State &state) {
  std::vector<std::byte> data(static_cast<std::size_t>(state.range(0) + state.range(1)));
  std::uint32_t seed = 0x12345678U; // xorshift32
  for(auto &byte : data) {
    seed ^= seed << 13U;
    seed ^= seed >> 17U;
    seed ^= seed << 5U;
    byte = static_cast<std::byte>(seed);
  }
  auto message = std::span<std::byte const>(data).subspan(static_cast<std::size_t>(state.range(1)));
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::fletcher_compute<Width>(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// Sizes from IPv4 and UDP headers to a large buffer, including the kernel thresholds; the second argument starts the message 1
// byte after an aligned address.
void sizes(benchmark::Benchmark *benchmark) { benchmark->ArgsProduct({{20, 64, 96, 128, 160, 192, 256, 1500, 4096, std::int64_t{1} << 20}, {0, 1}}); }

BENCHMARK_TEMPLATE(fletcher, 16)->Apply(sizes);
BENCHMARK_TEMPLATE(fletcher, 32)->Apply(sizes);
BENCHMARK_TEMPLATE(fletcher, 64)->Apply(sizes);

} // namespace

BENCHMARK_MAIN();
