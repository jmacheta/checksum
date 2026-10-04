// Throughput of the Internet checksum.

#include <checksum/internet.hpp>

#include <benchmark/benchmark.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

void internet(benchmark::State &state) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)));
  std::span<std::byte const> message = data;
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::internet_compute(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// IPv4 and UDP headers, a full Ethernet payload, a page and a large buffer.
BENCHMARK(internet)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);

} // namespace

BENCHMARK_MAIN();
