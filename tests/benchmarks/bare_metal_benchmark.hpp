#ifndef CHECKSUM_TESTS_BARE_METAL_BENCHMARK_HPP
#define CHECKSUM_TESTS_BARE_METAL_BENCHMARK_HPP

// Bare-metal stand-in for Google Benchmark, included as <benchmark/benchmark.h>: the part of its interface that the benchmarks
// use. Each benchmark runs a fixed number of iterations, best of 5 runs, and prints its cycles and instructions per call over
// semihosting where the core counts them.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace benchmark {

class State {
public:
  // What the range-for loop over a State yields.
  struct [[maybe_unused]] Value {};

  class iterator {
  public:
    Value operator*() const noexcept { return {}; }
    iterator &operator++() noexcept;
    bool operator!=(iterator const &other) const noexcept;

  private:
    friend class State;
    State *state_ = nullptr;
    std::int64_t remaining_ = 0;
  };

  State(std::string const &name, std::vector<std::int64_t> const &arguments, std::int64_t iterations);

  iterator begin() noexcept;
  iterator end() noexcept;
  std::int64_t range(std::size_t index = 0) const { return arguments_[index]; }
  std::int64_t iterations() const noexcept { return iterations_; }
  std::string name() const { return name_; }
  void SetBytesProcessed(std::int64_t bytes) noexcept { bytes_ = bytes; }

  std::uint32_t cycles = 0;
  std::uint32_t instructions = 0;
  std::int64_t bytes() const noexcept { return bytes_; }

private:
  std::string const &name_;
  std::vector<std::int64_t> const &arguments_;
  std::int64_t iterations_;
  std::int64_t bytes_ = 0;
};

class Benchmark {
public:
  Benchmark(std::string name, std::function<void(State &)> function);
  Benchmark *Arg(std::int64_t value);
  // ponytail: one call per benchmark, as the benchmarks make it.
  Benchmark *ArgsProduct(std::vector<std::vector<std::int64_t>> const &lists);

  std::string name;
  std::function<void(State &)> function;
  std::vector<std::vector<std::int64_t>> lists; // the arguments of every run are their Cartesian product
};

Benchmark *RegisterBenchmark(std::string const &name, std::function<void(State &)> function);

template <class Type> void DoNotOptimize(Type const &value) {
  if constexpr(std::is_trivially_copyable_v<Type> && sizeof(Type) <= sizeof(void *)) {
    asm volatile("" : : "r"(value) : "memory");
  } else {
    asm volatile("" : : "m"(value) : "memory");
  }
}

template <class Type> void DoNotOptimize(Type &value) {
  if constexpr(std::is_trivially_copyable_v<Type> && sizeof(Type) <= sizeof(void *)) {
    asm volatile("" : "+r"(value) : : "memory");
  } else {
    asm volatile("" : "+m"(value) : : "memory");
  }
}

// Takes --benchmark_filter=<name>, ^<prefix>, <suffix>$ or ^<name>$, and ignores the other --benchmark_ options.
void Initialize(int *argc, char **argv);
bool ReportUnrecognizedArguments(int argc, char **argv);
std::size_t RunSpecifiedBenchmarks();
inline void Shutdown() {}

} // namespace benchmark

#endif // CHECKSUM_TESTS_BARE_METAL_BENCHMARK_HPP
