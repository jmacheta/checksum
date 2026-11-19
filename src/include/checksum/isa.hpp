#ifndef CHECKSUM_ISA_HPP
#define CHECKSUM_ISA_HPP

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_isa Instruction sets
 *   Instruction-set presets: every algorithm takes one as a template parameter and runs the variant of its run-time
 *   loop that the library compiled for it, whatever the build's compiler flags. Constant evaluation runs the same portable code
 *   for every preset.
 *   @{
 */

namespace checksum {

/// Instruction-set preset: a set of CPU extensions that the library compiles a variant of every algorithm for. Each
/// preset includes the earlier ones of its line (x86, AArch64, 32-bit Arm), the two RISC-V presets are independent, and
/// values are only ever appended, never reordered.
enum class isa : std::uint8_t {
  generic,       ///< The default: only what every CPU of the architecture has (SSE2 on x86-64; NEON on little-endian AArch64 with GCC or Clang).
  x86_pclmul,    ///< x86-64 with SSE4.2 and PCLMULQDQ (SSSE3 included).
  x86_avx2,      ///< x86_pclmul plus AVX2.
  x86_vpclmul,   ///< x86_avx2 plus VPCLMULQDQ.
  x86_avxvnni,   ///< x86_vpclmul plus AVX-VNNI.
  aarch64_crc,   ///< AArch64 with the CRC32 instructions.
  aarch64_pmull, ///< aarch64_crc plus the AES extension (PMULL).
  thumb2,        ///< 32-bit Arm in Thumb-2 state.
  thumb2_dsp,    ///< thumb2 plus the DSP extension (ARMv7E-M, ARMv7-A).
  thumb2_neon,   ///< thumb2_dsp on ARMv7-A or later with NEON.
  thumb2_crc,    ///< thumb2_neon on ARMv8-A with the CRC32 instructions.
  riscv_zbc,     ///< RV64 with Zbc carry-less multiplication.
  riscv_v,       ///< RV64 with the V extension.
};

/// Whether this build has the variants of preset: generic always; x86-64 and AArch64 presets with GCC, Clang or MSVC; RISC-V
/// presets with GCC or Clang; 32-bit Arm presets with GCC only, thumb2 where the build's flags compile Thumb-2 code, thumb2_dsp
/// where they also have the DSP extension, thumb2_neon and thumb2_crc for A-profile ARMv7 and later with a hardware
/// floating-point ABI. Naming a preset it rejects is a compile error; the answer is the same in every translation unit.
[[nodiscard]] constexpr bool isa_available(isa preset) noexcept;

/// A set of presets that a CPU runs, for the dispatch engines of every algorithm (e.g. internet_dispatch_engine), which run
/// the best variant this build has inside the set. It always contains generic.
class isa_set {
public:
  /// generic alone.
  constexpr isa_set() noexcept = default;

  /// preset and the earlier presets of its line, so that an explicit choice falls back one rung at a time.
  constexpr isa_set(isa preset) noexcept; // NOLINT(google-explicit-constructor): a preset is the set of what it includes.

  /// Whether the set contains preset.
  [[nodiscard]] constexpr bool contains(isa preset) const noexcept;

  /// Both sets, e.g. isa_set{isa::riscv_zbc} | isa::riscv_v.
  [[nodiscard]] friend constexpr isa_set operator|(isa_set left, isa_set right) noexcept { return isa_set(left.bits | right.bits); }

  friend constexpr bool operator==(isa_set, isa_set) noexcept = default;

private:
  constexpr explicit isa_set(std::uint32_t bits) noexcept;

  std::uint32_t bits = 1; // bit n for the preset of value n
};

/// The presets this build has and the running CPU executes: what the CPU or the operating system reports, plus what the
/// compiler flags of the build already require; without an operating-system feature query (bare metal) only the latter.
/// Call it once and keep the result: it queries the CPU or the operating system every time.
[[nodiscard]] isa_set detect_isa() noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::isa_detail {

// The first preset of the line of preset; a preset without a line is its own.
constexpr isa line(isa preset) noexcept;

// Whether preset have runs everything preset want does: want is generic or an earlier preset of the same line.
constexpr bool includes(isa have, isa want) noexcept;

// The presets besides generic that this architecture and compiler build, ascending; the preset sources
// (checksum_private/presets.hpp) check the same macros.
#if (defined(__GNUC__) && defined(__x86_64__)) || (defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64) && !defined(_M_ARM64EC))
inline constexpr std::array built_presets{isa::x86_pclmul, isa::x86_avx2, isa::x86_vpclmul, isa::x86_avxvnni};
#elif (defined(__GNUC__) && defined(__aarch64__)) || (defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64))
inline constexpr std::array built_presets{isa::aarch64_crc, isa::aarch64_pmull};
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH >= 7 && __ARM_ARCH_PROFILE == 'A' && !defined(__SOFTFP__)
inline constexpr std::array built_presets{isa::thumb2, isa::thumb2_dsp, isa::thumb2_neon, isa::thumb2_crc};
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2 && defined(__ARM_FEATURE_DSP)
inline constexpr std::array built_presets{isa::thumb2, isa::thumb2_dsp};
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2
inline constexpr std::array built_presets{isa::thumb2};
#elif defined(__GNUC__) && defined(__riscv) && __riscv_xlen == 64
inline constexpr std::array built_presets{isa::riscv_zbc, isa::riscv_v};
#else
inline constexpr std::array<isa, 0> built_presets{};
#endif

// The preset of cpu that a dispatch engine runs: preferred (not generic) if this build has it and cpu contains it, else the
// latest preset of this build in cpu, else generic. Only RISC-V builds presets of two lines, which preferred settles for CRC.
constexpr isa select(isa_set cpu, isa preferred = isa::generic) noexcept;

// function.template operator()<preset>() for preset, a result of select() that a dispatch engine keeps: its switch over the
// presets of this build, the only work of a call besides the variant itself.
template <class Function> constexpr auto dispatch(isa preset, Function &&function);

// function.template operator()<preset>() if preset is built_presets[Index] or a later one, else for generic.
template <std::size_t Index, class Function> constexpr auto dispatch_from(isa preset, Function &function);

} // namespace checksum::isa_detail

///@}
///@}

namespace checksum::isa_detail {

constexpr isa line(isa preset) noexcept {
  switch(preset) {
  case isa::x86_avx2:
  case isa::x86_vpclmul:
  case isa::x86_avxvnni:
    return isa::x86_pclmul;
  case isa::aarch64_pmull:
    return isa::aarch64_crc;
  case isa::thumb2_dsp:
  case isa::thumb2_neon:
  case isa::thumb2_crc:
    return isa::thumb2;
  default:
    return preset;
  }
}

constexpr bool includes(isa have, isa want) noexcept { return want == isa::generic || (line(have) == line(want) && want <= have); }

} // namespace checksum::isa_detail

namespace checksum {

constexpr bool isa_available(isa preset) noexcept {
  if(preset == isa::generic) {
    return true;
  }
  for(isa const built : isa_detail::built_presets) {
    if(built == preset) {
      return true;
    }
  }
  return false;
}

constexpr isa_set::isa_set(isa preset) noexcept : bits(0) {
  for(auto value = static_cast<unsigned>(preset);; --value) {
    if(isa_detail::includes(preset, static_cast<isa>(value))) {
      bits |= 1U << value;
    }
    if(value == 0) {
      break;
    }
  }
}

constexpr isa_set::isa_set(std::uint32_t bits) noexcept : bits(bits) {}

constexpr bool isa_set::contains(isa preset) const noexcept { return ((bits >> static_cast<unsigned>(preset)) & 1U) != 0; }

} // namespace checksum

namespace checksum::isa_detail {

constexpr isa select(isa_set cpu, isa preferred) noexcept {
  if(preferred != isa::generic && isa_available(preferred) && cpu.contains(preferred)) {
    return preferred;
  }
  for(auto it = built_presets.rbegin(); it != built_presets.rend(); ++it) {
    if(cpu.contains(*it)) {
      return *it;
    }
  }
  return isa::generic;
}

template <std::size_t Index, class Function> constexpr auto dispatch_from(isa preset, Function &function) {
  if constexpr(Index == built_presets.size()) {
    return function.template operator()<isa::generic>();
  } else {
    constexpr isa candidate = built_presets[Index];
    if(preset == candidate) {
      return function.template operator()<candidate>();
    }
    return dispatch_from<Index + 1>(preset, function);
  }
}

template <class Function> constexpr auto dispatch(isa preset, Function &&function) { return dispatch_from<0>(preset, function); }

} // namespace checksum::isa_detail

#endif // CHECKSUM_ISA_HPP
