// Run-time CPU detection: the presets of this build that the running CPU executes. A source of its own, so that only
// programs that call detect_isa() link it.

#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#elif defined(__x86_64__)
#include <cpuid.h>
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

namespace checksum {

namespace {

// The presets this file checks on each architecture, and whether the running CPU executes one of them.
#if defined(__x86_64__) || defined(_M_X64)
constexpr std::array checked{isa::x86_pclmul, isa::x86_avx2, isa::x86_vpclmul, isa::x86_avxvnni};
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr std::array checked{isa::aarch64_crc, isa::aarch64_pmull};
#elif defined(__arm__)
constexpr std::array checked{isa::thumb2, isa::thumb2_dsp, isa::thumb2_neon, isa::thumb2_crc};
#elif defined(__riscv)
constexpr std::array checked{isa::riscv_zbc, isa::riscv_v};
#else
constexpr std::array<isa, 0> checked{};
#endif

consteval bool every_built_preset_checked() {
  for(isa preset : isa_detail::built_presets) {
    if(std::ranges::find(checked, preset) == checked.end()) {
      return false;
    }
  }
  return true;
}

static_assert(every_built_preset_checked(), "detect_isa() has no CPU check for a preset this build has");

bool runs(isa preset) noexcept {
#if defined(__x86_64__)
  bool const pclmul = __builtin_cpu_supports("sse4.2") && __builtin_cpu_supports("pclmul");
  bool const avx2 = pclmul && __builtin_cpu_supports("avx2");
  bool const vpclmul = avx2 && __builtin_cpu_supports("vpclmulqdq");
  // CPUID leaf 7 subleaf 1, EAX bit 4: Clang 18 does not know "avxvnni" in __builtin_cpu_supports.
  unsigned eax = 0;
  unsigned ebx = 0;
  unsigned ecx = 0;
  unsigned edx = 0;
  bool const avxvnni = vpclmul && __get_cpuid_count(7, 1, &eax, &ebx, &ecx, &edx) != 0 && ((eax >> 4U) & 1U) != 0;
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
  case isa::x86_pclmul:
    return pclmul;
  case isa::x86_avx2:
    return avx2;
  case isa::x86_vpclmul:
    return vpclmul;
  case isa::x86_avxvnni:
    return avxvnni;
  default:
    return false;
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
  return preset == isa::aarch64_pmull ? pmull : crc;
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
  return preset == isa::thumb2_crc ? crc : preset == isa::thumb2_neon ? neon : true;
#elif defined(__riscv)
#if defined(__linux__)
  if(preset == isa::riscv_v) {
    return (getauxval(AT_HWCAP) & (1UL << ('V' - 'A'))) != 0;
  }
#if __has_include(<asm/hwprobe.h>)
  riscv_hwprobe probe{.key = RISCV_HWPROBE_KEY_IMA_EXT_0, .value = 0};
  return syscall(__NR_riscv_hwprobe, &probe, 1, 0, nullptr, 0) == 0 && (probe.value & RISCV_HWPROBE_EXT_ZBC) != 0;
#else
  return true;
#endif
#else
  return true;
#endif
#else
  static_cast<void>(preset);
  return false;
#endif
}

} // namespace

isa_set detect_isa() noexcept {
  isa_set result;
  for(isa const preset : checked) {
    if(isa_available(preset) && runs(preset)) {
      result = result | preset;
    }
  }
  return result;
}

} // namespace checksum
