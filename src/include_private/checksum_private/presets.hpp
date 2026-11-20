#ifndef CHECKSUM_PRIVATE_PRESETS_HPP
#define CHECKSUM_PRIVATE_PRESETS_HPP

// Target options of each instruction-set preset that this architecture and compiler build; a preset source compiles its
// preset only where the macro is defined. isa_detail::built_presets in checksum/isa.hpp checks the same macros.
// clang-cl defines __clang__ and the architecture macros of Clang but not __GNUC__: it builds Clang's presets.
// NOLINTBEGIN(cppcoreguidelines-macro-usage): tested by #ifdef and passed to target pragmas.

#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
#define CHECKSUM_TARGET_X86_PCLMUL "sse4.2,pclmul"
#define CHECKSUM_TARGET_X86_AVX2 "sse4.2,pclmul,avx2"
#define CHECKSUM_TARGET_X86_VPCLMUL "sse4.2,pclmul,avx2,vpclmulqdq"
#define CHECKSUM_TARGET_X86_AVXVNNI "sse4.2,pclmul,avx2,vpclmulqdq,avxvnni"
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#define CHECKSUM_TARGET_AARCH64_CRC "+crc"
#define CHECKSUM_TARGET_AARCH64_PMULL "+crc+aes"
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2 && (defined(__thumb2__) || !defined(__clang__))
// GCC inlines the build's always_inline std functions only into code whose target includes the build's, and "arch="
// replaces the build's architecture, so a preset the build already has only selects Thumb state. Thumb-1 builds have no
// Thumb-2 presets: GNU as checks Thumb-2 immediates against the architecture at the end of the file, which GCC resets to
// the build's. Clang inlines no ARM-state function into Thumb code, so it builds the presets only in a Thumb-2 build; its
// target attribute takes no 32-bit Arm architecture or FPU, and LLVM 20 and 21 crash on NEON code where "neon" is added
// to a build without it, so it builds thumb2_neon and thumb2_crc only with the build's own NEON.
#define CHECKSUM_TARGET_THUMB2 "thumb"
#ifdef __ARM_FEATURE_DSP
#define CHECKSUM_TARGET_THUMB2_DSP "thumb"
#endif
// NEON on A-profile cores with a hardware floating-point ABI, added to the build's own floating-point unit.
#if __ARM_ARCH >= 7 && __ARM_ARCH_PROFILE == 'A' && !defined(__SOFTFP__) && (defined(__ARM_NEON) || !defined(__clang__))
#ifdef __ARM_NEON
#define CHECKSUM_TARGET_THUMB2_NEON "thumb"
#elif __ARM_ARCH >= 8
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon-fp-armv8,thumb"
#elifdef __ARM_FEATURE_FMA
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon-vfpv4,thumb"
#else
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon,thumb"
#endif
#if defined(__ARM_FEATURE_CRC32) && defined(__ARM_NEON)
#define CHECKSUM_TARGET_THUMB2_CRC "thumb"
#elifdef __clang__
#define CHECKSUM_TARGET_THUMB2_CRC "thumb,crc"
#else
#define CHECKSUM_TARGET_THUMB2_CRC "arch=armv8-a+crc,fpu=neon-fp-armv8,thumb"
#endif
#endif
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__riscv) && __riscv_xlen == 64
#define CHECKSUM_TARGET_RISCV_ZBC "arch=+zbc"
#define CHECKSUM_TARGET_RISCV_V "arch=+v"
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64) && !defined(_M_ARM64EC)
// MSVC compiles intrinsics without target options, so target_begin.hpp ignores these.
#define CHECKSUM_TARGET_X86_PCLMUL ""
#define CHECKSUM_TARGET_X86_AVX2 ""
#define CHECKSUM_TARGET_X86_VPCLMUL ""
#define CHECKSUM_TARGET_X86_AVXVNNI ""
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#define CHECKSUM_TARGET_AARCH64_CRC ""
#define CHECKSUM_TARGET_AARCH64_PMULL ""
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)

// CHECKSUM_PRESET_HAS(NEON), (DSP) or (RISCV_V): 1 if the preset that CHECKSUM_PRESET names compiles that kernel section,
// whose intrinsic types need the extension; 0 elsewhere. AArch64 always has NEON and tests CHECKSUM_ARM64 instead.
// NOLINTBEGIN(cppcoreguidelines-macro-usage,cppcoreguidelines-macro-to-enum,modernize-macro-to-enum,readability-identifier-naming): tested by #if,
// named after the preset.
#define CHECKSUM_PRESET_HAS(feature) CHECKSUM_PRESET_HAS_EXPANDED(feature, CHECKSUM_PRESET)
#define CHECKSUM_PRESET_HAS_EXPANDED(feature, preset) CHECKSUM_PRESET_HAS_NAME(feature, preset)
#define CHECKSUM_PRESET_HAS_NAME(feature, preset) CHECKSUM_PRESET_HAS_##feature##_##preset
#define CHECKSUM_PRESET_HAS_DSP_thumb2_dsp 1
#define CHECKSUM_PRESET_HAS_NEON_thumb2_neon 1
#define CHECKSUM_PRESET_HAS_NEON_thumb2_crc 1
#define CHECKSUM_PRESET_HAS_RISCV_V_riscv_v 1
// NOLINTEND(cppcoreguidelines-macro-usage,cppcoreguidelines-macro-to-enum,modernize-macro-to-enum,readability-identifier-naming)

#endif // CHECKSUM_PRIVATE_PRESETS_HPP
