// Throughput of the Fletcher checksums, for every preset and through the dispatch engine.

#include <checksum/fletcher.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace {

template <class Engine> void fletcher(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); }, static_cast<std::size_t>(state.range(1)));
}

// Also the kernel thresholds; the second argument starts the message 1 byte after an aligned address.
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {96, 128, 160, 192}, true); }

template <unsigned Width> void register_width() {
  isa_bench::register_presets(
      "fletcher" + std::to_string(Width), [](auto preset) { return fletcher<checksum::fletcher_engine<Width, decltype(preset)::value>>; },
      fletcher<checksum::fletcher_dispatch_engine<Width>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_width<16>();
  register_width<32>();
  register_width<64>();
  return isa_bench::run(argc, argv);
}
