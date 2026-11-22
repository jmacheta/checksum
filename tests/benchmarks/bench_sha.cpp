// Throughput of SHA-1, SHA-256 and SHA-512, for every preset and through the dispatch engine.

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

template <checksum::sha_variant Variant> void register_variant(std::string const &name) {
  isa_bench::register_presets(
      name, [](auto preset) { return sha<checksum::sha_engine<Variant, decltype(preset)::value>>; }, sha<checksum::sha_dispatch_engine<Variant>>,
      sizes);
}

} // namespace

int main(int argc, char **argv) {
  register_variant<checksum::sha_variant::sha1>("sha1");
  register_variant<checksum::sha_variant::sha256>("sha256");
  register_variant<checksum::sha_variant::sha512>("sha512");
  return isa_bench::run(argc, argv);
}
