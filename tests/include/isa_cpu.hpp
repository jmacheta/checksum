#ifndef CHECKSUM_TESTS_ISA_CPU_HPP
#define CHECKSUM_TESTS_ISA_CPU_HPP

// Instruction-set presets for tests and benchmarks: their names, the presets this build has, and whether the running CPU
// executes each one.

#include <checksum/isa.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <string>
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
template <class Function> void for_each_built_preset(Function const &function) {
  [&]<std::size_t... Index>(std::index_sequence<Index...>) {
    (function.template operator()<built_preset<Index>>(), ...);
  }(std::make_index_sequence<built_count>{});
}

// The last enumerator of checksum::isa.
inline constexpr checksum::isa last_preset = checksum::isa::aarch64_sha512;

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
  case x86_avx512:
    return "x86_avx512";
  case x86_sha:
    return "x86_sha";
  case aarch64_sha2:
    return "aarch64_sha2";
  case aarch64_sha512:
    return "aarch64_sha512";
  }
  return {};
}

// Whether the running CPU executes the code of preset: the library's detect_isa(), queried once.
inline bool cpu_runs(checksum::isa preset) {
  static checksum::isa_set const cpu = checksum::detect_isa();
  return cpu.contains(preset);
}

// The presets of a comma-separated list of preset names, e.g. "aarch64_pmull,riscv_v", each with the presets it extends;
// none if a name is unknown.
inline std::optional<checksum::isa_set> named_presets(std::string_view names) {
  checksum::isa_set result;
  for(auto const part : std::views::split(names, ',')) {
    std::string_view const word(part.begin(), part.end());
    bool known = false;
    for(unsigned value = 0; value <= static_cast<unsigned>(last_preset); ++value) {
      if(name(static_cast<checksum::isa>(value)) == word) {
        result = result | static_cast<checksum::isa>(value);
        known = true;
      }
    }
    if(!known) {
      return std::nullopt;
    }
  }
  return result;
}

// The value of the environment variable, or none if it is unset or empty.
inline std::optional<std::string> environment(char const *variable) {
#ifdef _MSC_VER
  char *value = nullptr;
  std::size_t size = 0;
  std::optional<std::string> result;
  if(_dupenv_s(&value, &size, variable) == 0 && value != nullptr && *value != '\0') {
    result = value;
  }
  std::free(value); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): _dupenv_s allocates with malloc
  return result;
#else
  char const *const value = std::getenv(variable); // NOLINT(concurrency-mt-unsafe): no thread sets it
  return value != nullptr && *value != '\0' ? std::optional<std::string>(value) : std::nullopt;
#endif
}

// The presets that CHECKSUM_REQUIRED_ISA names (see named_presets): a CI job sets it to what its CPU guarantees, so that
// tests fail instead of skipping those presets. generic alone if unset; none if it names an unknown preset.
inline std::optional<checksum::isa_set> required_presets() {
  std::optional<std::string> const names = environment("CHECKSUM_REQUIRED_ISA");
  return names ? named_presets(*names) : checksum::isa_set{};
}

// Whether a run must not skip preset; true for every preset if CHECKSUM_REQUIRED_ISA names an unknown one.
inline bool required(checksum::isa preset) {
  static std::optional<checksum::isa_set> const presets = required_presets();
  return !presets || presets->contains(preset);
}

// Whether the CPU runs preset, for benchmarks that register only what runs; ends the program if it cannot and
// CHECKSUM_REQUIRED_ISA requires preset.
inline bool cpu_runs_or_exit(checksum::isa preset) {
  if(!cpu_runs(preset) && required(preset)) {
    std::fprintf(stderr, "the CPU cannot run %.*s, which CHECKSUM_REQUIRED_ISA requires\n", static_cast<int>(name(preset).size()),
                 name(preset).data());
    std::exit(EXIT_FAILURE); // NOLINT(concurrency-mt-unsafe): before any benchmark thread starts
  }
  return cpu_runs(preset);
}

} // namespace isa_cpu

#endif // CHECKSUM_TESTS_ISA_CPU_HPP
