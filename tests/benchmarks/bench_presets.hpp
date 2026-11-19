#ifndef CHECKSUM_TESTS_BENCH_PRESETS_HPP
#define CHECKSUM_TESTS_BENCH_PRESETS_HPP

// Benchmarks of every instruction-set preset: generic under the plain name, every other preset this build has and the CPU
// runs as <name>_<preset>.

#include <checksum/isa.hpp>

#include <benchmark/benchmark.h>
#include <isa_cpu.hpp>

#include <string>
#include <type_traits>

namespace isa_bench {

// Registers make(std::integral_constant<isa, Preset>{}), a callable taking benchmark::State &, for generic and every runnable
// preset, then lets arguments add the sizes.
template <class Make, class Arguments> void register_presets(std::string const &name, Make make, Arguments arguments) {
  auto const one = [&]<checksum::isa Preset>() {
    if(!isa_cpu::cpu_runs_or_exit(Preset)) {
      return;
    }
    std::string const full = Preset == checksum::isa::generic ? name : name + "_" + std::string(isa_cpu::name(Preset));
    arguments(benchmark::RegisterBenchmark(full, make(std::integral_constant<checksum::isa, Preset>{})));
  };
  one.template operator()<checksum::isa::generic>();
  isa_cpu::for_each_built_preset([&]<checksum::isa Preset>() {
    if constexpr(Preset != checksum::isa::generic) {
      one.template operator()<Preset>();
    }
  });
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
