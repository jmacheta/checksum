// Throughput of XXH32, XXH64, XXH3-64 and XXH3-128.

#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <class Hash> void run(benchmark::State &state, Hash hash) {
  std::vector<std::byte> data(static_cast<std::size_t>(state.range(0)));
  std::uint32_t seed = 0x12345678U; // xorshift32
  for(auto &byte : data) {
    seed ^= seed << 13U;
    seed ^= seed >> 17U;
    seed ^= seed << 5U;
    byte = static_cast<std::byte>(seed);
  }
  std::span<std::byte const> message = data;
  for(auto _ : state) {
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(hash(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void xxhash32(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxhash_compute<32>(data); });
}

void xxhash64(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxhash_compute<64>(data); });
}

void xxh3_64(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<64>(data); });
}

void xxh3_128(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<128>(data); });
}

// IPv4 and UDP headers, a cache line, a short packet, a full Ethernet payload, a page and a large buffer.
BENCHMARK(xxhash32)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxhash64)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_64)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_128)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);

} // namespace

BENCHMARK_MAIN();
