#ifndef CHECKSUM_PRIVATE_ARCH_HPP
#define CHECKSUM_PRIVATE_ARCH_HPP

// Selects the directory of CPU kernels: defines one of CHECKSUM_ARCH_X86_64, CHECKSUM_ARCH_ARM, CHECKSUM_ARCH_RISCV64 and
// CHECKSUM_ARCH_GENERIC. Each architecture header checks its instruction-set extensions: without them it uses no kernel.

#if defined(CHECKSUM_ACCELERATION) && !CHECKSUM_ACCELERATION
#define CHECKSUM_ARCH_GENERIC 1
#elif defined(__x86_64__)
#define CHECKSUM_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(__arm__)
#define CHECKSUM_ARCH_ARM 1
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define CHECKSUM_ARCH_RISCV64 1
#else
#define CHECKSUM_ARCH_GENERIC 1
#endif

#endif // CHECKSUM_PRIVATE_ARCH_HPP
