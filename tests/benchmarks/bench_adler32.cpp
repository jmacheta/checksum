// Throughput of the Adler-32 checksum, for every preset and through the dispatch engine.

#include <checksum/adler32.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>

namespace {

template <class Engine> void adler32(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); }, static_cast<std::size_t>(state.range(1)));
}

// Also the kernel thresholds; the second argument starts the message 1 byte after an aligned address.
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {96, 128, 160, 192}, true); }

} // namespace

int main(int argc, char **argv) {
  isa_bench::register_presets(
      "adler32", [](auto preset) { return adler32<checksum::adler32_engine<decltype(preset)::value>>; }, adler32<checksum::adler32_dispatch_engine>,
      sizes);
  return isa_bench::run(argc, argv);
}
