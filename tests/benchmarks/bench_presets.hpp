#ifndef CHECKSUM_TESTS_BENCH_PRESETS_HPP
#define CHECKSUM_TESTS_BENCH_PRESETS_HPP

// Shared by every benchmark executable. A benchmark is named <name>/<size>[/<offset>] for isa::generic, <name>_<preset>/... for every
// other preset this build has and the CPU runs, and <name>_dispatch/... for the dispatch engine of the detected presets. Only
// Fletcher and Adler-32 add the offset, 0 or 1 bytes after an aligned address (sizes(..., true)). Before it measures, every
// benchmark registered with a reference checks that both give the same results, and the program exits with 1 if not.

#include <checksum/isa.hpp>

#include <benchmark/benchmark.h>
#include <isa_cpu.hpp>
#include <test_data.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace isa_bench {

// Hashes the same random message of state.range(0) bytes, starting offset bytes after an aligned address, in every iteration.
// While checking, measure() hashes these lengths, the benchmark's size and both offsets into checked_results instead.
inline bool checking = false;
inline std::vector<std::byte> checked_results;
inline constexpr std::array<std::size_t, 10> checked_sizes{0, 1, 3, 15, 16, 63, 64, 255, 1500, 4097};

template <class Hash> void check(benchmark::State const &state, Hash hash) {
  std::size_t const size = static_cast<std::size_t>(state.range(0));
  std::vector<std::byte> const data = random_bytes(std::max(size, checked_sizes.back()) + 1);
  checked_results.clear();
  for(std::size_t const offset : {0UZ, 1UZ}) {
    for(std::size_t const length : checked_sizes) {
      auto const result = hash(std::span<std::byte const>(data).subspan(offset, length));
      auto const bytes = std::as_bytes(std::span(&result, 1));
      checked_results.insert(checked_results.end(), bytes.begin(), bytes.end());
    }
    auto const result = hash(std::span<std::byte const>(data).subspan(offset, size));
    auto const bytes = std::as_bytes(std::span(&result, 1));
    checked_results.insert(checked_results.end(), bytes.begin(), bytes.end());
  }
}

template <class Hash> void measure(benchmark::State &state, Hash hash, std::size_t offset = 0) {
  if(checking) {
    check(state, hash);
    return;
  }
  std::vector<std::byte> const data = random_bytes(static_cast<std::size_t>(state.range(0)) + offset);
  auto message = std::span<std::byte const>(data).subspan(offset);
  for(auto _ : state) { // NOLINT(readability-identifier-length): the idiom of Google Benchmark
    benchmark::DoNotOptimize(message);
    benchmark::DoNotOptimize(hash(message));
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// Registers function as name. Before every run it checks that function hashes like reference, the same benchmark function
// for isa::generic, and exits with 1 if not.
template <class Function, class Reference> benchmark::Benchmark *register_checked(std::string const &name, Function function, Reference reference) {
  return benchmark::RegisterBenchmark(name, [function, reference](benchmark::State &state) {
    checking = true;
    reference(state);
    std::vector<std::byte> const expected = checked_results;
    function(state);
    checking = false;
    if(checked_results != expected) {
      std::fprintf(stderr, "result mismatch with isa::generic: %s, size %lld\n", state.name().c_str(), static_cast<long long>(state.range(0)));
      std::exit(EXIT_FAILURE); // NOLINT(concurrency-mt-unsafe): benchmarks run on one thread
    }
    function(state);
  });
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
// for_each_preset(), and dispatch as <name>_dispatch, each checked against generic; arguments adds the sizes to each.
template <class Make, class Dispatch, class Arguments>
void register_presets(std::string const &name, Make make, Dispatch dispatch, Arguments arguments) {
  auto const generic = make(std::integral_constant<checksum::isa, checksum::isa::generic>{});
  for_each_preset([&]<checksum::isa Preset>(std::string const &suffix) {
    arguments(register_checked(name + suffix, make(std::integral_constant<checksum::isa, Preset>{}), generic));
  });
  arguments(register_checked(name + "_dispatch", dispatch, generic));
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
