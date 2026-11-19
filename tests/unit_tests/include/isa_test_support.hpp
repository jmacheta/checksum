#ifndef CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP
#define CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP

// Test helpers for instruction-set presets: the list of presets this build has, and a typed-test fixture over them that
// skips the presets the CPU cannot run.

#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <isa_cpu.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace isa_test {

using isa_cpu::cpu_runs;
using isa_cpu::name;

template <checksum::isa Preset> using preset = std::integral_constant<checksum::isa, Preset>;

template <std::size_t... Index> auto built_presets(std::index_sequence<Index...>) -> testing::Types<preset<isa_cpu::built_preset<Index>>...>;

// Every preset but generic that this build has; generic alone if there are none.
using presets = decltype(built_presets(std::make_index_sequence<isa_cpu::built_count>{}));

// Names the typed tests after their preset.
struct preset_name {
  template <class Preset> static std::string GetName(int /*index*/) { return std::string(name(Preset::value)); }
};

// Skips the running test if the CPU cannot run preset; fails it if cpu_runs() has no check for preset.
inline void skip_unless_cpu_runs(checksum::isa preset) {
  std::optional<bool> const runs = cpu_runs(preset);
  if(!runs.has_value()) {
    FAIL() << "isa_cpu::cpu_runs() has no check for " << name(preset);
  }
  if(!*runs) {
    GTEST_SKIP() << "the CPU cannot run " << name(preset);
  }
}

// Typed-test fixture over presets: skips a preset the CPU cannot run.
template <class Preset> class preset_test : public testing::Test {
protected:
  void SetUp() override { skip_unless_cpu_runs(Preset::value); }
};

} // namespace isa_test

#endif // CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP
