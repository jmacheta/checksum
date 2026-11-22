// Bare-metal stand-in for Google Benchmark: runs the registered benchmarks and prints one line each over semihosting. Under
// a bare-metal preset, Initialize() also checks that detect_isa() reports the presets that the compiler flags promise.

#include "bare_metal_benchmark.hpp"

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <checksum_private/compiled_isa.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string_view>

namespace {

constexpr std::int64_t iterations_per_run = 8;
constexpr int runs = 5;

std::deque<benchmark::Benchmark> benchmarks;
std::string_view filter;

// The cycle and instruction counters of the core, where it has them: the DWT cycle counter of Armv7-M and Armv8-M Mainline,
// the PMU of Armv7-A and -R and Armv8-R. Armv6-M has neither.
bool has_cycles = false;
bool has_instructions = false;

#if __ARM_ARCH_PROFILE == 'M' && __ARM_ARCH_ISA_THUMB == 2
volatile std::uint32_t &register_at(std::uintptr_t address) {
  return *reinterpret_cast<std::uint32_t volatile *>(address); // NOLINT(performance-no-int-to-ptr): a system register
}
auto &demcr = register_at(0xE000EDFC);
auto &dwt_ctrl = register_at(0xE0001000);
auto &dwt_cyccnt = register_at(0xE0001004);

void start_counters() {
  demcr = demcr | (1U << 24U); // TRCENA
  // QEMU keeps TRCENA clear: it has no DWT.
  if((demcr & (1U << 24U)) != 0 && (dwt_ctrl & (1U << 25U)) == 0) { // NOCYCCNT
    dwt_cyccnt = 0;
    dwt_ctrl = dwt_ctrl | 1U; // CYCCNTENA
    has_cycles = true;
  }
}
std::uint32_t read_cycles() { return has_cycles ? dwt_cyccnt : 0; }
std::uint32_t read_instructions() { return 0; }
#elif __ARM_ARCH_PROFILE == 'A' || __ARM_ARCH_PROFILE == 'R'
void start_counters() {
  std::uint32_t control = 0;
  asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(control)); // PMCR
  // Event 0x08, instruction architecturally executed: Cortex-R5 and -R52 count it, Cortex-A9 does not.
  has_instructions = __ARM_ARCH_PROFILE == 'R' && ((control >> 11U) & 0x1FU) != 0;
  asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(control | 7U));                                 // enable, reset the counters
  asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(0U));                                           // PMSELR: event counter 0
  asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(0x08U));                                        // PMXEVTYPER
  asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(has_instructions ? 0x80000001U : 0x80000000U)); // PMCNTENSET
  has_cycles = true;
}
std::uint32_t read_cycles() {
  std::uint32_t value = 0;
  asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(value)); // PMCCNTR
  return value;
}
std::uint32_t read_instructions() {
  std::uint32_t value = 0;
  if(has_instructions) {
    asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(value)); // PMXEVCNTR of counter 0
  }
  return value;
}
#else
void start_counters() {}
std::uint32_t read_cycles() { return 0; }
std::uint32_t read_instructions() { return 0; }
#endif

#ifdef CHECKSUM_EXPECTED_ISA
using checksum::isa;
using checksum::isa_set;

constexpr isa_set expected_set{isa::CHECKSUM_EXPECTED_ISA};
constexpr isa_set expected_built{isa::CHECKSUM_EXPECTED_BUILT};

// The presets of this build that compiler_guarantees() reports: what detect_isa() returns without a feature query.
consteval isa_set guaranteed_presets() {
  isa_set result;
  for(isa const preset : checksum::isa_detail::built_presets) {
    if(checksum::isa_detail::compiler_guarantees(preset)) {
      result = result | preset;
    }
  }
  return result;
}

// Whether isa_available() accepts exactly generic and the presets of the line up to CHECKSUM_EXPECTED_BUILT.
consteval bool built_as_expected() {
  for(unsigned value = 0; value <= static_cast<unsigned>(isa::riscv_v); ++value) {
    if(checksum::isa_available(static_cast<isa>(value)) != expected_built.contains(static_cast<isa>(value))) {
      return false;
    }
  }
  return true;
}

static_assert(guaranteed_presets() == expected_set, "the compiler flags promise other presets than CHECKSUM_EXPECTED_ISA");
static_assert(built_as_expected(), "the build has other presets than CHECKSUM_EXPECTED_BUILT and those before it");

// Whether the dispatch engine made for cpu selects the preset that dispatch promises, one that cpu contains.
bool selects_for(isa_set cpu) {
  isa const preset = checksum::internet_dispatch_engine(cpu).preset();
  return preset == checksum::isa_detail::select(cpu, checksum::internet_detail::own_loops) && cpu.contains(preset);
}

void check_isa() {
  isa_set const detected = checksum::detect_isa();
  bool passed = detected == expected_set &&
                checksum::internet_dispatch_engine{}.preset() == checksum::isa_detail::select(detected, checksum::internet_detail::own_loops);
  passed = passed && selects_for(detected);
  for(isa const preset : checksum::isa_detail::built_presets) {
    passed = passed && (!detected.contains(preset) || selects_for(preset));
  }
  if(!passed) {
    std::puts("FAIL: detect_isa() or the dispatch engines do not match CHECKSUM_EXPECTED_ISA");
    std::exit(EXIT_FAILURE);
  }
}
#else
void check_isa() {}
#endif

// Whether name matches the filter: a substring, or a prefix with ^, a suffix with $, the whole name with both.
bool selected(std::string_view name) {
  std::string_view pattern = filter;
  bool const prefix = pattern.starts_with('^');
  bool const suffix = pattern.ends_with('$');
  pattern.remove_prefix(prefix ? 1 : 0);
  pattern.remove_suffix(suffix ? 1 : 0);
  if(prefix && suffix) {
    return name == pattern;
  }
  if(prefix) {
    return name.starts_with(pattern);
  }
  return suffix ? name.ends_with(pattern) : name.contains(pattern);
}

// Prints value / divisor with two decimals, or n/a.
void print_ratio(bool available, std::uint64_t value, std::uint64_t divisor, int width) {
  if(!available || divisor == 0) {
    iprintf(" %*s", width, "n/a");
    return;
  }
  std::uint64_t const hundredths = (value * 100U) / divisor;
  iprintf(" %*lu.%02lu", width - 3, static_cast<unsigned long>(hundredths / 100U), static_cast<unsigned long>(hundredths % 100U));
}

// Runs benchmark with arguments, best of 5 runs, and prints its line.
void run(benchmark::Benchmark const &benchmark, std::vector<std::int64_t> const &arguments) {
  std::string name = benchmark.name;
  for(std::int64_t const argument : arguments) {
    name += '/' + std::to_string(argument);
  }
  if(!selected(name)) {
    return;
  }
  // The benchmark allocates its message; skip sizes that the heap cannot hold, e.g. 1 MiB on a microcontroller.
  void *const probe = std::malloc(static_cast<std::size_t>(arguments.front()) + 8192U);
  if(probe == nullptr) {
    iprintf("%-40s skipped: not enough memory\n", name.c_str());
    return;
  }
  std::free(probe);
  benchmark::State best(name, arguments, iterations_per_run);
  best.cycles = UINT32_MAX;
  for(int index = 0; index < runs; ++index) {
    benchmark::State state(name, arguments, iterations_per_run);
    benchmark.function(state);
    if(state.cycles <= best.cycles) {
      best.cycles = state.cycles;
      best.instructions = state.instructions;
      best.SetBytesProcessed(state.bytes());
    }
  }
  iprintf("%-40s", name.c_str());
  print_ratio(has_cycles, best.cycles, iterations_per_run, 12);
  print_ratio(has_cycles, best.cycles, static_cast<std::uint64_t>(best.bytes()), 10);
  print_ratio(has_instructions, best.instructions, iterations_per_run, 14);
  iprintf("\n");
}

} // namespace

namespace benchmark {

State::State(std::string const &name, std::vector<std::int64_t> const &arguments, std::int64_t iterations)
    : name_(name), arguments_(arguments), iterations_(iterations) {}

State::iterator State::begin() noexcept {
  iterator result;
  result.state_ = this;
  result.remaining_ = iterations_;
  instructions = read_instructions();
  cycles = read_cycles();
  return result;
}

State::iterator State::end() noexcept { return {}; }

State::iterator &State::iterator::operator++() noexcept {
  --remaining_;
  return *this;
}

bool State::iterator::operator!=(iterator const & /*other*/) const noexcept {
  if(remaining_ != 0) {
    return true;
  }
  state_->cycles = read_cycles() - state_->cycles;
  state_->instructions = read_instructions() - state_->instructions;
  return false;
}

Benchmark::Benchmark(std::string name, std::function<void(State &)> function) : name(std::move(name)), function(std::move(function)) {}

Benchmark *Benchmark::Arg(std::int64_t value) {
  if(lists.empty()) {
    lists.emplace_back();
  }
  lists.front().push_back(value);
  return this;
}

Benchmark *Benchmark::ArgsProduct(std::vector<std::vector<std::int64_t>> const &new_lists) {
  lists = new_lists;
  return this;
}

Benchmark *RegisterBenchmark(std::string const &name, std::function<void(State &)> function) {
  return &benchmarks.emplace_back(name, std::move(function));
}

void Initialize(int *argc, char **argv) {
  check_isa();
  start_counters();
  int kept = *argc > 0 ? 1 : 0;
  for(int index = kept; index < *argc; ++index) {
    std::string_view const argument = argv[index];
    if(argument.starts_with("--benchmark_filter=")) {
      filter = argument.substr(std::strlen("--benchmark_filter="));
    } else if(!argument.starts_with("--benchmark_")) {
      argv[kept++] = argv[index];
    }
  }
  *argc = kept;
}

bool ReportUnrecognizedArguments(int argc, char **argv) {
  for(int index = 1; index < argc; ++index) {
    iprintf("unrecognized argument: %s\n", argv[index]);
  }
  return argc > 1;
}

std::size_t RunSpecifiedBenchmarks() {
  iprintf("%-40s %12s %10s %14s\n", "benchmark", "cycles", "cycles/B", "instructions");
  for(Benchmark const &benchmark : benchmarks) {
    std::vector<std::int64_t> arguments(benchmark.lists.size());
    std::vector<std::size_t> position(benchmark.lists.size());
    // Every combination of the lists, the last one fastest, as Google Benchmark orders them.
    for(bool more = !benchmark.lists.empty(); more;) {
      for(std::size_t list = 0; list < arguments.size(); ++list) {
        arguments[list] = benchmark.lists[list][position[list]];
      }
      run(benchmark, arguments);
      more = false;
      for(std::size_t list = arguments.size(); list-- > 0 && !more;) {
        more = ++position[list] < benchmark.lists[list].size();
        if(!more) {
          position[list] = 0;
        }
      }
    }
  }
  return benchmarks.size();
}

} // namespace benchmark
