#ifndef CHECKSUM_TESTS_ISA_CPU_HPP
#define CHECKSUM_TESTS_ISA_CPU_HPP

// Instruction-set presets for tests and benchmarks: their names, the presets this build has, and whether the running CPU
// executes each one.

#include <checksum/isa.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif
#if defined(_MSC_VER) && defined(_M_ARM64)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#if defined(__linux__) && (defined(__aarch64__) || defined(__arm__) || defined(__riscv))
#include <sys/auxv.h>
#endif
#if defined(__linux__) && defined(__riscv) && __has_include(<asm/hwprobe.h>)
#include <asm/hwprobe.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace isa_cpu {

// The number of presets besides generic that this build has, from the range in isa.hpp; 1 (generic) if there are none.
inline constexpr std::size_t built_count =
    static_cast<std::size_t>(checksum::isa_detail::last_built) - static_cast<std::size_t>(checksum::isa_detail::first_built) + 1;

// The Index-th preset of that range.
template <std::size_t Index>
inline constexpr checksum::isa built_preset = static_cast<checksum::isa>(static_cast<std::size_t>(checksum::isa_detail::first_built) + Index);

// Calls function.template operator()<Preset>() for every preset of that range, so that a new preset is covered once it is built.
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

// Whether the running CPU executes the code of preset, or nullopt where this function has no check for it. Operating
// systems without a feature query (bare metal) report every built preset as runnable.
inline std::optional<bool> cpu_runs(checksum::isa preset) {
  using enum checksum::isa;
  if(preset == generic) {
    return true;
  }
#if defined(__x86_64__)
  bool const pclmul = __builtin_cpu_supports("sse4.2") && __builtin_cpu_supports("pclmul");
  bool const avx2 = pclmul && __builtin_cpu_supports("avx2");
  bool const vpclmul = avx2 && __builtin_cpu_supports("vpclmulqdq");
  bool const avxvnni = vpclmul && __builtin_cpu_supports("avxvnni");
#elif defined(_M_X64)
  // CPUID leaf 1, leaf 7 and leaf 7 subleaf 1 as EAX, EBX, ECX, EDX; AVX also needs the OS to save the YMM registers.
  std::array<int, 4> leaf1{};
  std::array<int, 4> leaf7{};
  std::array<int, 4> leaf7_1{};
  __cpuid(leaf1.data(), 1);
  __cpuidex(leaf7.data(), 7, 0);
  __cpuidex(leaf7_1.data(), 7, 1);
  auto const bit = [](int value, unsigned index) { return ((static_cast<unsigned>(value) >> index) & 1U) != 0; };
  bool const ymm_saved = bit(leaf1[2], 27) && (_xgetbv(0) & 6U) == 6U;
  bool const pclmul = bit(leaf1[2], 20) && bit(leaf1[2], 1);
  bool const avx2 = pclmul && ymm_saved && bit(leaf7[1], 5);
  bool const vpclmul = avx2 && bit(leaf7[2], 10);
  bool const avxvnni = vpclmul && bit(leaf7_1[0], 4);
#endif
#if defined(__x86_64__) || defined(_M_X64)
  switch(preset) {
  case x86_pclmul:
    return pclmul;
  case x86_avx2:
    return avx2;
  case x86_vpclmul:
    return vpclmul;
  case x86_avxvnni:
    return avxvnni;
  default:
    return std::nullopt;
  }
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(__linux__)
  unsigned long const hwcap = getauxval(AT_HWCAP);
  bool const crc = (hwcap & HWCAP_CRC32) != 0;
  bool const pmull = crc && (hwcap & HWCAP_PMULL) != 0;
#elif defined(_M_ARM64)
  bool const crc = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE) != 0;
  bool const pmull = crc && IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) != 0;
#else
  bool const crc = true;
  bool const pmull = true;
#endif
  switch(preset) {
  case aarch64_crc:
    return crc;
  case aarch64_pmull:
    return pmull;
  default:
    return std::nullopt;
  }
#elif defined(__arm__)
  // thumb2 and thumb2_dsp exist only where the build's own flags have them, so the CPU runs them.
#if defined(__linux__)
  constexpr unsigned long hwcap2_crc32 = 1UL << 4U; // HWCAP2_CRC32 of the kernel's asm/hwcap.h
  bool const neon = (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) != 0;
  bool const crc = neon && (getauxval(AT_HWCAP2) & hwcap2_crc32) != 0;
#else
  bool const neon = true;
  bool const crc = true;
#endif
  switch(preset) {
  case thumb2:
  case thumb2_dsp:
    return true;
  case thumb2_neon:
    return neon;
  case thumb2_crc:
    return crc;
  default:
    return std::nullopt;
  }
#elif defined(__riscv)
#if defined(__linux__)
  bool const vector = (getauxval(AT_HWCAP) & (1UL << ('V' - 'A'))) != 0;
#if __has_include(<asm/hwprobe.h>)
  riscv_hwprobe probe{.key = RISCV_HWPROBE_KEY_IMA_EXT_0, .value = 0};
  bool const zbc = syscall(__NR_riscv_hwprobe, &probe, 1, 0, nullptr, 0) == 0 && (probe.value & RISCV_HWPROBE_EXT_ZBC) != 0;
#else
  bool const zbc = true;
#endif
#else
  bool const vector = true;
  bool const zbc = true;
#endif
  switch(preset) {
  case riscv_zbc:
    return zbc;
  case riscv_v:
    return vector;
  default:
    return std::nullopt;
  }
#else
  return std::nullopt;
#endif
}

} // namespace isa_cpu

#endif // CHECKSUM_TESTS_ISA_CPU_HPP
