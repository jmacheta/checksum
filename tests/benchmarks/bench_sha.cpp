// Throughput of SHA-1, SHA-256 and SHA-512 with both strategies, for every preset and through the dispatch engine.

#include <checksum/sha.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace {

template <class Engine> void sha(benchmark::State &state) {
  Engine const engine{};
  isa_bench::measure(state, [&](std::span<std::byte const> data) { return engine.compute(data); });
}

void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark); }

template <checksum::sha_variant Variant, class Strategy> void register_variant(std::string const &name) {
  isa_bench::register_presets(
      name, [](auto preset) { return sha<checksum::sha_engine<Variant, Strategy, decltype(preset)::value>>; },
      sha<checksum::sha_dispatch_engine<Variant, Strategy>>, sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_variant<checksum::sha_variant::sha1, checksum::sha_compact>("sha1");
  register_variant<checksum::sha_variant::sha256, checksum::sha_compact>("sha256");
  register_variant<checksum::sha_variant::sha512, checksum::sha_compact>("sha512");
  register_variant<checksum::sha_variant::sha1, checksum::sha_unrolled>("sha1_unrolled");
  register_variant<checksum::sha_variant::sha256, checksum::sha_unrolled>("sha256_unrolled");
  register_variant<checksum::sha_variant::sha512, checksum::sha_unrolled>("sha512_unrolled");
  return isa_bench::run(argc, argv);
}
