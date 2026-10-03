// Throughput of MurmurHash3_x86_32 and MurmurHash3_x64_128.

#include <checksum/murmur3.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <unsigned Width> void run(benchmark::State &state) {
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
    benchmark::DoNotOptimize(checksum::murmur3_compute<Width>(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void murmur3_32(benchmark::State &state) { run<32>(state); }

void murmur3_128(benchmark::State &state) { run<128>(state); }

// IPv4 and UDP headers, a cache line, a full Ethernet payload, a page and a large buffer.
BENCHMARK(murmur3_32)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(murmur3_128)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);

} // namespace

BENCHMARK_MAIN();
