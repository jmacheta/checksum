// Throughput of the ZFS fletcher4 checksum, for every preset.

#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>
#include <test_data.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <checksum::isa Isa> void fletcher4(benchmark::State &state) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)));
  std::span<std::byte const> message = data;
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(checksum::fletcher4_compute<Isa>(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// IPv4 and UDP headers, the lane thresholds, a full Ethernet payload, a page and a large buffer.
void sizes(benchmark::Benchmark *benchmark) {
  benchmark->Arg(20)->Arg(64)->Arg(192)->Arg(256)->Arg(384)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
}

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets("fletcher4", [](auto preset) { return fletcher4<decltype(preset)::value>; }, sizes);
  return isa_bench::run(argc, argv);
}
