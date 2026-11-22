// Run-time CPU detection: the presets of this build that the running CPU executes. A source of its own, so that only
// programs that call detect_isa() link it.

#include <checksum/isa.hpp>
#include <checksum_private/compiled_isa.hpp>

#include <algorithm>
#include <array>
#include <cstddef> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep

// MSVC and clang-cl query CPUID through <intrin.h>: __builtin_cpu_supports needs compiler-rt, which clang-cl does not link.
#ifdef _MSC_VER
#include <intrin.h>
#elifdef __x86_64__
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
#if defined(__linux__) && defined(__riscv)
#include <unistd.h>
#endif
#if defined(__FreeBSD__) && (defined(__aarch64__) || defined(__arm__))
#include <sys/auxv.h>
#endif
#if defined(__APPLE__) && defined(__aarch64__)
#include <sys/sysctl.h>
#endif

namespace checksum {

namespace {

// The presets this file checks on each architecture, and whether the running CPU executes one of them.
#if defined(__x86_64__) || defined(_M_X64)
constexpr std::array checked{isa::x86_pclmul, isa::x86_avx2, isa::x86_vpclmul, isa::x86_avxvnni};
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr std::array checked{isa::aarch64_crc, isa::aarch64_pmull};
#elifdef __arm__
constexpr std::array checked{isa::thumb2, isa::thumb2_dsp, isa::thumb2_neon, isa::thumb2_crc};
#elifdef __riscv
constexpr std::array checked{isa::riscv_zbc, isa::riscv_v};
#else
constexpr std::array<isa, 0> checked{};
#endif

consteval bool every_built_preset_checked() {
  return std::ranges::all_of(isa_detail::built_presets, [](isa preset) { return std::ranges::find(checked, preset) != checked.end(); });
}

static_assert(every_built_preset_checked(), "detect_isa() has no CPU check for a preset this build has");

#if defined(__APPLE__) && defined(__aarch64__)
// A hw.optional sysctl of macOS and iOS; false where the system lacks it.
bool apple_feature(char const *name) noexcept {
  int value = 0;
  std::size_t size = sizeof(value);
  return sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value != 0;
}
#endif

// Whether the CPU or the operating system reports that preset runs; false where neither can be asked.
bool runs(isa preset) noexcept {
#ifdef _M_X64
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
#elifdef __x86_64__
  bool const pclmul = __builtin_cpu_supports("sse4.2") && __builtin_cpu_supports("pclmul");
  bool const avx2 = pclmul && __builtin_cpu_supports("avx2");
  bool const vpclmul = avx2 && __builtin_cpu_supports("vpclmulqdq");
  // CPUID leaf 7 subleaf 1, EAX bit 4: Clang 18 does not know "avxvnni" in __builtin_cpu_supports.
  unsigned eax = 0;
  unsigned ebx = 0;
  unsigned ecx = 0;
  unsigned edx = 0;
  bool const avxvnni = vpclmul && __get_cpuid_count(7, 1, &eax, &ebx, &ecx, &edx) != 0 && ((eax >> 4U) & 1U) != 0;
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
#ifdef __linux__
  unsigned long const hwcap = getauxval(AT_HWCAP);
  bool const crc = (hwcap & HWCAP_CRC32) != 0;
  bool const pmull = crc && (hwcap & HWCAP_PMULL) != 0;
#elifdef _M_ARM64
  bool const crc = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE) != 0;
  bool const pmull = crc && IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) != 0;
#elif defined(__FreeBSD__) && defined(HWCAP_CRC32) && defined(HWCAP_PMULL)
  unsigned long hwcap = 0;
  bool const known = elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap)) == 0;
  bool const crc = known && (hwcap & HWCAP_CRC32) != 0;
  bool const pmull = crc && (hwcap & HWCAP_PMULL) != 0;
#elifdef __APPLE__
  bool const crc = apple_feature("hw.optional.armv8_crc32");
  bool const pmull = crc && apple_feature("hw.optional.arm.FEAT_PMULL");
#else
  bool const crc = false;
  bool const pmull = false;
#endif
  return preset == isa::aarch64_pmull ? pmull : crc;
#elifdef __arm__
  // thumb2 and thumb2_dsp exist only where the build's own flags have them: compiler_guarantees() reports them.
#ifdef __linux__
  constexpr unsigned long hwcap2_crc32 = 1UL << 4U; // HWCAP2_CRC32 of the kernel's asm/hwcap.h
  bool const neon = (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) != 0;
  bool const crc = neon && (getauxval(AT_HWCAP2) & hwcap2_crc32) != 0;
#elif defined(__FreeBSD__) && defined(HWCAP_NEON) && defined(HWCAP2_CRC32) && defined(AT_HWCAP2)
  unsigned long hwcap = 0;
  unsigned long hwcap2 = 0;
  bool const neon = elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap)) == 0 && (hwcap & HWCAP_NEON) != 0;
  bool const crc = neon && elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2)) == 0 && (hwcap2 & HWCAP2_CRC32) != 0;
#else
  bool const neon = false;
  bool const crc = false;
#endif
  return preset == isa::thumb2_crc ? crc : preset == isa::thumb2_neon && neon;
#elifdef __riscv
#ifdef __linux__
  if(preset == isa::riscv_v) {
    return (getauxval(AT_HWCAP) & (1UL << ('V' - 'A'))) != 0;
  }
  // The kernel ABI's riscv_hwprobe (Linux 6.4) and its Zbc bit (Linux 6.8), which toolchains with older kernel headers
  // lack. An older kernel fails the call or leaves the bit clear.
  constexpr long hwprobe_call = 258;
  constexpr std::int64_t ima_ext_0_key = 4;
  constexpr std::uint64_t zbc_bit = 1U << 7U;
  struct {
    std::int64_t key;
    std::uint64_t value;
  } probe{.key = ima_ext_0_key, .value = 0};
  return preset == isa::riscv_zbc && syscall(hwprobe_call, &probe, 1, 0, nullptr, 0) == 0 && (probe.value & zbc_bit) != 0;
#else
  static_cast<void>(preset);
  return false;
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
    if(isa_available(preset) && (isa_detail::compiler_guarantees(preset) || runs(preset))) {
      result = result | preset;
    }
  }
  return result;
}

} // namespace checksum
