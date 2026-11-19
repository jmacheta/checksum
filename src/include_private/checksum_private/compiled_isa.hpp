#ifndef CHECKSUM_PRIVATE_COMPILED_ISA_HPP
#define CHECKSUM_PRIVATE_COMPILED_ISA_HPP

// The presets that the compiler flags of the build already require of the CPU: what detect_isa() reports where it has no
// operating-system feature query.

#include <checksum/isa.hpp>

namespace checksum::isa_detail {

// Whether the predefined macros of the build promise every extension of preset and of the earlier presets of its line.
constexpr bool compiler_guarantees(isa preset) noexcept;

constexpr bool compiler_guarantees(isa preset) noexcept {
  switch(preset) {
  case isa::generic:
    return true;
#if defined(__SSE4_2__) && defined(__PCLMUL__)
  case isa::x86_pclmul:
    return true;
#if defined(__AVX2__)
  case isa::x86_avx2:
    return true;
#if defined(__VPCLMULQDQ__)
  case isa::x86_vpclmul:
    return true;
#if defined(__AVXVNNI__)
  case isa::x86_avxvnni:
    return true;
#endif
#endif
#endif
#endif
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
  case isa::aarch64_crc:
    return true;
#if defined(__ARM_FEATURE_AES) || defined(__ARM_FEATURE_CRYPTO)
  case isa::aarch64_pmull:
    return true;
#endif
#endif
#if defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2
  case isa::thumb2:
    return true;
#if defined(__ARM_FEATURE_DSP)
  case isa::thumb2_dsp:
    return true;
#if defined(__ARM_NEON)
  case isa::thumb2_neon:
    return true;
#if defined(__ARM_FEATURE_CRC32)
  case isa::thumb2_crc:
    return true;
#endif
#endif
#endif
#endif
#if defined(__riscv) && defined(__riscv_zbc)
  case isa::riscv_zbc:
    return true;
#endif
#if defined(__riscv) && defined(__riscv_v)
  case isa::riscv_v:
    return true;
#endif
  default:
    return false;
  }
}

} // namespace checksum::isa_detail

#endif // CHECKSUM_PRIVATE_COMPILED_ISA_HPP
