// Throughput of XXH32, XXH64, XXH3-64 and XXH3-128, of seeded XXH3-64 and of XXH3-64 streamed in pieces.

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
  run(state, [](std::span<std::byte const> data) { return checksum::xxh32_compute(data); });
}

void xxhash64(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh64_compute(data); });
}

void xxh3_64(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<64>(data); });
}

void xxh3_64_seeded(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<64>(data, 0x9E3779B97F4A7C15U); });
}

// The message folded in pieces of Piece bytes.
template <std::size_t Piece> void xxh3_64_streaming(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) {
    checksum::xxh3_state<64> hash{};
    for(; data.size() > Piece; data = data.subspan(Piece)) {
      hash = checksum::xxh3_update(hash, data.first(Piece));
    }
    return checksum::xxh3_finalize(checksum::xxh3_update(hash, data));
  });
}

void xxh3_128(benchmark::State &state) {
  run(state, [](std::span<std::byte const> data) { return checksum::xxh3_compute<128>(data); });
}

// IPv4 and UDP headers, a cache line, a short packet, a full Ethernet payload, a page and a large buffer.
BENCHMARK(xxhash32)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxhash64)->Arg(20)->Arg(64)->Arg(256)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
// XXH3 also at 200 bytes (129-240 class) and 512 bytes (two buffers).
BENCHMARK(xxh3_64)->Arg(20)->Arg(64)->Arg(200)->Arg(256)->Arg(512)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_64_seeded)->Arg(20)->Arg(200)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_64_streaming<64>)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_64_streaming<4096>)->Arg(std::int64_t{1} << 20);
BENCHMARK(xxh3_128)->Arg(20)->Arg(64)->Arg(200)->Arg(256)->Arg(512)->Arg(1500)->Arg(4096)->Arg(std::int64_t{1} << 20);

} // namespace

BENCHMARK_MAIN();
