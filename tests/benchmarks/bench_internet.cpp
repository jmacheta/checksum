// Throughput of the Internet checksum, for every preset and through the dispatch engine of the detected presets.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <checksum::isa Isa> void internet(benchmark::State &state) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)));
  std::span<std::byte const> message = data;
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::internet_compute<Isa>(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// The dispatch engine of the detected presets: against internet_<preset>, the cost of its switch at every call, which
// short inputs (20 and 64 bytes) show.
void internet_dispatch(benchmark::State &state) {
  checksum::internet_dispatch_engine const engine;
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)));
  std::span<std::byte const> message = data;
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(engine.compute(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// IPv4 and UDP headers, a full Ethernet payload, a page and a large buffer.
void sizes(benchmark::Benchmark *benchmark) { benchmark->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20); }

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets("internet", [](auto preset) { return internet<decltype(preset)::value>; }, sizes);
  sizes(benchmark::RegisterBenchmark("internet_dispatch", internet_dispatch));
  return isa_bench::run(argc, argv);
}
