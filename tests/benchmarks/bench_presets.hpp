#ifndef CHECKSUM_TESTS_BENCH_PRESETS_HPP
#define CHECKSUM_TESTS_BENCH_PRESETS_HPP

// Shared by every benchmark executable. A benchmark is named <name>/<size>[/<offset>] for isa::generic, <name>_<preset>/... for every
// other preset this build has and the CPU runs, and <name>_dispatch/... for the dispatch engine of the detected presets. Only
// Fletcher and Adler-32 add the offset, 0 or 1 bytes after an aligned address (sizes(..., true)).

#include <checksum/isa.hpp>

#include <benchmark/benchmark.h>
#include <isa_cpu.hpp>
#include <test_data.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace isa_bench {

// Hashes the same random message of state.range(0) bytes, starting offset bytes after an aligned address, in every iteration.
template <class Hash> void measure(benchmark::State &state, Hash hash, std::size_t offset = 0) {
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)) + offset);
  auto message = std::span<std::byte const>(data).subspan(offset);
  for(auto _ : state) { // NOLINT(readability-identifier-length): the idiom of Google Benchmark
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(hash(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// Calls function.template operator()<Preset>(suffix) for generic with an empty suffix, then for every other preset this build
// has and the CPU runs with the suffix _<preset>.
template <class Function> void for_each_preset(Function function) {
  auto const one = [&]<checksum::isa Preset>() {
    if(isa_cpu::cpu_runs_or_exit(Preset)) {
      function.template operator()<Preset>(Preset == checksum::isa::generic ? std::string() : "_" + std::string(isa_cpu::name(Preset)));
    }
  };
  one.template operator()<checksum::isa::generic>();
  isa_cpu::for_each_built_preset([&]<checksum::isa Preset>() {
    if constexpr(Preset != checksum::isa::generic) {
      one.template operator()<Preset>();
    }
  });
}

// Registers make(std::integral_constant<isa, Preset>{}), a callable taking benchmark::State &, for every preset of
// for_each_preset(), and dispatch as <name>_dispatch; arguments adds the sizes to each.
template <class Make, class Dispatch, class Arguments>
void register_presets(std::string const &name, Make make, Dispatch dispatch, Arguments arguments) {
  for_each_preset([&]<checksum::isa Preset>(std::string const &suffix) {
    arguments(benchmark::RegisterBenchmark(name + suffix, make(std::integral_constant<checksum::isa, Preset>{})));
  });
  arguments(benchmark::RegisterBenchmark(name + "_dispatch", dispatch));
}

// The sizes of every algorithm: IPv4 and UDP headers, a cache line, a short packet, a full Ethernet payload, a page and a
// large buffer. extra adds sizes that matter to one algorithm, e.g. its kernel thresholds; unaligned adds a second argument,
// the offset of measure(), 0 and 1.
inline void sizes(benchmark::Benchmark *benchmark, std::initializer_list<std::int64_t> extra = {}, bool unaligned = false) {
  std::vector<std::int64_t> all{20, 64, 256, 1500, 4096, std::int64_t{1024} * 1024};
  all.insert(all.end(), extra);
  std::ranges::sort(all);
  if(unaligned) {
    benchmark->ArgsProduct({all, {0, 1}});
  } else {
    benchmark->ArgsProduct({all});
  }
}

// BENCHMARK_MAIN() for benchmarks registered at run time.
inline int run(int argc, char **argv) {
  benchmark::Initialize(&argc, argv);
  if(benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}

} // namespace isa_bench

#endif // CHECKSUM_TESTS_BENCH_PRESETS_HPP
