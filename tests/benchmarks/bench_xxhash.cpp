// Throughput of XXH32 and XXH64, for every preset and through the dispatch engine.

#include <checksum/xxhash.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace {

template <class Engine> void xxhash(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark); }

template <unsigned Width> void register_width() {
  isa_bench::register_presets(
      "xxhash" + std::to_string(Width), [](auto preset) { return xxhash<checksum::xxhash_engine<Width, decltype(preset)::value>>; },
      xxhash<checksum::xxhash_dispatch_engine<Width>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_width<32>();
  register_width<64>();
  return isa_bench::run(argc, argv);
}
