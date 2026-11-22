// Throughput of the ZFS fletcher4 checksum, for every preset and through the dispatch engine.

#include <checksum/fletcher4.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>

namespace {

template <class Engine> void fletcher4(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

// Also the lane thresholds.
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {192, 384}); }

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets(
      "fletcher4", [](auto preset) { return fletcher4<checksum::fletcher4_engine<decltype(preset)::value>>; },
      fletcher4<checksum::fletcher4_dispatch_engine>, sizes);
  return isa_bench::run(argc, argv);
}
