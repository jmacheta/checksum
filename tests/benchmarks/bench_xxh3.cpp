// Throughput of XXH3-64 and XXH3-128, for every preset and through the dispatch engine, of seeded XXH3-64 and of XXH3-64
// streamed in pieces.

#include <checksum/xxh3.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace {

template <class Engine> void xxh3(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void xxh3_64_seeded(benchmark::State &state) {
  isa_bench::measure(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<64>(data, 0x9E3779B97F4A7C15U); });
}

// The message folded in pieces of Piece bytes.
template <std::size_t Piece> void xxh3_64_streaming(benchmark::State &state) {
  isa_bench::measure(state, [](std::span<std::byte const> data) {
    constexpr checksum::xxh3_engine<64> engine;
    checksum::xxh3_state<64> hash{};
    for(; data.size() > Piece; data = data.subspan(Piece)) {
      hash = engine.update(hash, data.first(Piece));
    }
    return engine.finalize(engine.update(hash, data));
  });
}

// Also 200 bytes (the 129-240 class) and 512 bytes (two buffers).
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {200, 512}); }

template <unsigned Width> void register_width() {
  isa_bench::register_presets(
      "xxh3_" + std::to_string(Width), [](auto preset) { return xxh3<checksum::xxh3_engine<Width, decltype(preset)::value>>; },
      xxh3<checksum::xxh3_dispatch_engine<Width>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_width<64>();
  register_width<128>();
  sizes(benchmark::RegisterBenchmark("xxh3_64_seeded", xxh3_64_seeded));
  benchmark::RegisterBenchmark("xxh3_64_streaming<64>", xxh3_64_streaming<64>)->Arg(std::int64_t{1024} * 1024);
  benchmark::RegisterBenchmark("xxh3_64_streaming<4096>", xxh3_64_streaming<4096>)->Arg(std::int64_t{1024} * 1024);
  return isa_bench::run(argc, argv);
}
