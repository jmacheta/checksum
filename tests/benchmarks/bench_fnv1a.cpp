// Throughput of FNV-1a 32 and 64, for every preset and through the dispatch engine.

#include <checksum/fnv1a.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace {

template <class Engine> void fnv1a(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark); }

template <unsigned Width> void register_width() {
  isa_bench::register_presets(
      "fnv1a_" + std::to_string(Width), [](auto preset) { return fnv1a<checksum::fnv1a_engine<Width, decltype(preset)::value>>; },
      fnv1a<checksum::fnv1a_dispatch_engine<Width>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_width<32>();
  register_width<64>();
  return isa_bench::run(argc, argv);
}
