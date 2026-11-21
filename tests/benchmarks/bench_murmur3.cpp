// Throughput of MurmurHash3_x86_32 and MurmurHash3_x64_128, for every preset and through the dispatch engine.

#include <checksum/murmur3.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace {

template <class Engine> void murmur3(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark); }

template <unsigned Width> void register_width() {
  isa_bench::register_presets(
      "murmur3_" + std::to_string(Width), [](auto preset) { return murmur3<checksum::murmur3_engine<Width, decltype(preset)::value>>; },
      murmur3<checksum::murmur3_dispatch_engine<Width>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_width<32>();
  register_width<128>();
  return isa_bench::run(argc, argv);
}
