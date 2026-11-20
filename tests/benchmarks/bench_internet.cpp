// Throughput of the Internet checksum, for every preset and through the dispatch engine.

#include <checksum/internet.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>

namespace {

template <class Engine> void internet(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark); }

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets(
      "internet", [](auto preset) { return internet<checksum::internet_engine<decltype(preset)::value>>; },
      internet<checksum::internet_dispatch_engine>, sizes);
  return isa_bench::run(argc, argv);
}
