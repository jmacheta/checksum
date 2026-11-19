#ifndef CHECKSUM_ISA_HPP
#define CHECKSUM_ISA_HPP

#include <cstdint>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_isa Instruction sets
 *   Instruction-set presets: every algorithm takes one as a template parameter and runs the variant of its run-time
 *   loop that the library compiled for it, whatever the build's compiler flags.
 *   @{
 */

namespace checksum {

/// Instruction-set preset: a set of CPU extensions that the library compiles a variant of every algorithm for. Each
/// preset of an architecture includes the earlier ones of its line (x86, AArch64, 32-bit Arm); the two RISC-V presets
/// are independent. Values are only ever appended, never reordered.
enum class isa : std::uint8_t {
  generic,       ///< The default: only what every CPU of the architecture has (SSE2 on x86-64, NEON on little-endian AArch64).
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

/// Whether this build has the variants of preset: generic always, other presets on their architecture with GCC or Clang,
/// and with MSVC on x64 and ARM64.
/// On 32-bit Arm, GCC only: thumb2 where the build's flags compile Thumb-2 code, thumb2_dsp where they also have the DSP
/// extension, thumb2_neon and thumb2_crc for A-profile ARMv7 and later with a hardware floating-point ABI. Naming a
/// preset it rejects is a compile error; the answer is the same in every translation unit.
[[nodiscard]] constexpr bool isa_available(isa preset) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::isa_detail {

// The first preset of the line of preset; a preset without a line is its own.
constexpr isa line(isa preset) noexcept;

// Whether preset have runs everything preset want does: want is generic or an earlier preset of the same line.
constexpr bool includes(isa have, isa want) noexcept;

// The presets besides generic that this architecture and compiler build, a range of the enumeration; the preset
// sources (checksum_private/presets.hpp) check the same macros.
#if (defined(__GNUC__) && defined(__x86_64__)) || (defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64) && !defined(_M_ARM64EC))
inline constexpr isa first_built = isa::x86_pclmul;
inline constexpr isa last_built = isa::x86_avxvnni;
#elif (defined(__GNUC__) && defined(__aarch64__)) || (defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64))
inline constexpr isa first_built = isa::aarch64_crc;
inline constexpr isa last_built = isa::aarch64_pmull;
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH >= 7 && __ARM_ARCH_PROFILE == 'A' && !defined(__SOFTFP__)
inline constexpr isa first_built = isa::thumb2;
inline constexpr isa last_built = isa::thumb2_crc;
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2 && defined(__ARM_FEATURE_DSP)
inline constexpr isa first_built = isa::thumb2;
inline constexpr isa last_built = isa::thumb2_dsp;
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2
inline constexpr isa first_built = isa::thumb2;
inline constexpr isa last_built = isa::thumb2;
#elif defined(__GNUC__) && defined(__riscv) && __riscv_xlen == 64
inline constexpr isa first_built = isa::riscv_zbc;
inline constexpr isa last_built = isa::riscv_v;
#else
inline constexpr isa first_built = isa::generic;
inline constexpr isa last_built = isa::generic;
#endif

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
  return preset == isa::generic || (isa_detail::first_built <= preset && preset <= isa_detail::last_built);
}

} // namespace checksum

#endif // CHECKSUM_ISA_HPP
