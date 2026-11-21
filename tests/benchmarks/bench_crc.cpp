// Throughput of the built-in strategies, crc_lut_sliced and crc_lut_braided also for every preset and through the dispatch
// engine. Benchmarks are named <strategy>[_<preset>]/<crc>/<size>.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/isa.hpp>

#include "bench_presets.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace {

using namespace checksum;

// Also 16 and 32 bytes, the short inputs of doc/performance.md.
void sizes(benchmark::Benchmark *benchmark) { isa_bench::sizes(benchmark, {16, 32}); }

// crc_engine_for<Parameters, Strategy, Isa>, or the dispatch engine of Parameters and Strategy if Dispatch.
template <crc_parameters Parameters, class Strategy, isa Isa, bool Dispatch> void measure_engine(benchmark::State &state) {
  if constexpr(Dispatch) {
    using dispatch_engine = std::optional<crc_dispatch_engine<crc_register_for<Parameters.polynomial.width()>, Strategy>>;
    // Created straight on the heap: a 64-bit braided engine is 33 KB, too big to copy around a microcontroller's stack.
    std::unique_ptr<dispatch_engine const> const crc(new dispatch_engine const(dispatch_engine::value_type::create(Parameters)));
    isa_bench::measure(state, [&](std::span<std::byte const> data) { return (*crc)->compute(data); });
  } else {
    isa_bench::measure(state, [](std::span<std::byte const> data) { return crc_engine_for<Parameters, Strategy, Isa>.compute(data); });
  }
}

// Checked against the generic crc_lut_byte engine.
template <crc_parameters Parameters, class Strategy, isa Isa, bool Dispatch> void register_engine(std::string const &strategy, char const *name) {
  sizes(isa_bench::register_checked(strategy + "/" + name, measure_engine<Parameters, Strategy, Isa, Dispatch>,
                                    measure_engine<Parameters, crc_lut_byte, isa::generic, false>));
}

// Widths 8, 16, 32 and 64, each with reflected and unreflected input.
template <class Strategy, isa Isa = isa::generic, bool Dispatch = false> void register_strategy(std::string const &strategy) {
  register_engine<crc8_smbus, Strategy, Isa, Dispatch>(strategy, "crc8_smbus");
  register_engine<crc8_maxim_dow, Strategy, Isa, Dispatch>(strategy, "crc8_maxim_dow");
  register_engine<crc16_xmodem, Strategy, Isa, Dispatch>(strategy, "crc16_xmodem");
  register_engine<crc16_kermit, Strategy, Isa, Dispatch>(strategy, "crc16_kermit");
  register_engine<crc32_bzip2, Strategy, Isa, Dispatch>(strategy, "crc32_bzip2");
  register_engine<crc32_iso_hdlc, Strategy, Isa, Dispatch>(strategy, "crc32_iso_hdlc");
  register_engine<crc32c, Strategy, Isa, Dispatch>(strategy, "crc32c");
  register_engine<crc64_ecma_182, Strategy, Isa, Dispatch>(strategy, "crc64_ecma_182");
  register_engine<crc64_xz, Strategy, Isa, Dispatch>(strategy, "crc64_xz");
}

} // namespace

int main(int argc, char **argv) {
  register_strategy<crc_lut_none>("none");
  register_strategy<crc_lut_nibble>("nibble");
  register_strategy<crc_lut_byte>("byte");
  isa_bench::for_each_preset([]<isa Preset>(std::string const &suffix) { register_strategy<crc_lut_sliced, Preset>("sliced" + suffix); });
  register_strategy<crc_lut_sliced, isa::generic, true>("sliced_dispatch");
  isa_bench::for_each_preset([]<isa Preset>(std::string const &suffix) { register_strategy<crc_lut_braided, Preset>("braided" + suffix); });
  register_strategy<crc_lut_braided, isa::generic, true>("braided_dispatch");
  return isa_bench::run(argc, argv);
}
