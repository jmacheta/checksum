#ifndef CHECKSUM_TESTS_ISA_CPU_HPP
#define CHECKSUM_TESTS_ISA_CPU_HPP

// Instruction-set presets for tests and benchmarks: their names, the presets this build has, and whether the running CPU
// executes each one.

#include <checksum/isa.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

namespace isa_cpu {

// The number of presets that tests cover: those of isa_detail::built_presets, or 1 (generic alone) if there are none.
inline constexpr std::size_t built_count = checksum::isa_detail::built_presets.empty() ? 1 : checksum::isa_detail::built_presets.size();

// The Index-th of them; generic if there are none.
template <std::size_t Index>
inline constexpr checksum::isa built_preset =
    checksum::isa_detail::built_presets.empty() ? checksum::isa::generic : checksum::isa_detail::built_presets[Index];

// Calls function.template operator()<Preset>() for every preset of isa_detail::built_presets, so that a new preset is covered once it is built.
template <class Function> void for_each_built_preset(Function &&function) {
  [&]<std::size_t... Index>(std::index_sequence<Index...>) {
    (function.template operator()<built_preset<Index>>(), ...);
  }(std::make_index_sequence<built_count>{});
}

// The enumerator's name, or an empty view for a preset this list lacks; ut_isa.cpp checks every built preset has one.
constexpr std::string_view name(checksum::isa preset) noexcept {
  using enum checksum::isa;
  switch(preset) {
  case generic:
    return "generic";
  case x86_pclmul:
    return "x86_pclmul";
  case x86_avx2:
    return "x86_avx2";
  case x86_vpclmul:
    return "x86_vpclmul";
  case x86_avxvnni:
    return "x86_avxvnni";
  case aarch64_crc:
    return "aarch64_crc";
  case aarch64_pmull:
    return "aarch64_pmull";
  case thumb2:
    return "thumb2";
  case thumb2_dsp:
    return "thumb2_dsp";
  case thumb2_neon:
    return "thumb2_neon";
  case thumb2_crc:
    return "thumb2_crc";
  case riscv_zbc:
    return "riscv_zbc";
  case riscv_v:
    return "riscv_v";
  }
  return {};
}

// Whether the running CPU executes the code of preset: the library's detect_isa(), queried once.
inline bool cpu_runs(checksum::isa preset) {
  static checksum::isa_set const cpu = checksum::detect_isa();
  return cpu.contains(preset);
}

} // namespace isa_cpu

#endif // CHECKSUM_TESTS_ISA_CPU_HPP
