#ifndef CHECKSUM_PRIVATE_PRESETS_HPP
#define CHECKSUM_PRIVATE_PRESETS_HPP

// Target options of each instruction-set preset that this architecture and compiler build; a preset source compiles its
// preset only where the macro is defined. isa_detail::first_built and last_built in checksum/isa.hpp check the same macros.

#if defined(__GNUC__) && defined(__x86_64__)
#define CHECKSUM_TARGET_X86_PCLMUL "sse4.2,pclmul"
#define CHECKSUM_TARGET_X86_AVX2 "sse4.2,pclmul,avx2"
#define CHECKSUM_TARGET_X86_VPCLMUL "sse4.2,pclmul,avx2,vpclmulqdq"
#define CHECKSUM_TARGET_X86_AVXVNNI "sse4.2,pclmul,avx2,vpclmulqdq,avxvnni"
#elif defined(__GNUC__) && defined(__aarch64__)
#define CHECKSUM_TARGET_AARCH64_CRC "+crc"
#define CHECKSUM_TARGET_AARCH64_PMULL "+crc+aes"
#elif defined(__GNUC__) && !defined(__clang__) && defined(__arm__) && __ARM_ARCH_ISA_THUMB >= 2
// GCC inlines the build's always_inline std functions only into code whose target includes the build's, and "arch="
// replaces the build's architecture, so a preset the build already has only selects Thumb state. Thumb-1 builds have no
// Thumb-2 presets: GNU as checks Thumb-2 immediates against the architecture at the end of the file, which GCC resets to
// the build's. Clang's target attribute takes no 32-bit Arm architecture.
#define CHECKSUM_TARGET_THUMB2 "thumb"
#if defined(__ARM_FEATURE_DSP)
#define CHECKSUM_TARGET_THUMB2_DSP "thumb"
#endif
// NEON on A-profile cores with a hardware floating-point ABI, added to the build's own floating-point unit.
#if __ARM_ARCH >= 7 && __ARM_ARCH_PROFILE == 'A' && !defined(__SOFTFP__)
#if defined(__ARM_NEON)
#define CHECKSUM_TARGET_THUMB2_NEON "thumb"
#elif __ARM_ARCH >= 8
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon-fp-armv8,thumb"
#elif defined(__ARM_FEATURE_FMA)
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon-vfpv4,thumb"
#else
#define CHECKSUM_TARGET_THUMB2_NEON "fpu=neon,thumb"
#endif
#if defined(__ARM_FEATURE_CRC32) && defined(__ARM_NEON)
#define CHECKSUM_TARGET_THUMB2_CRC "thumb"
#else
#define CHECKSUM_TARGET_THUMB2_CRC "arch=armv8-a+crc,fpu=neon-fp-armv8,thumb"
#endif
#endif
#elif defined(__GNUC__) && defined(__riscv) && __riscv_xlen == 64
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

#endif // CHECKSUM_PRIVATE_PRESETS_HPP
