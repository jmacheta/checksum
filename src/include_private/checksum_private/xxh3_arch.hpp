#ifndef CHECKSUM_PRIVATE_XXH3_ARCH_HPP
#define CHECKSUM_PRIVATE_XXH3_ARCH_HPP

// CPU kernel of the XXH3 stripe loop, from the directory of the target architecture; without the instruction-set extensions, or with
// CHECKSUM_ACCELERATION defined to 0, the portable kernel folds everything. Each architecture header defines:
// - stripe_kernel_available; if true, stripe_kernel with the members of portable_kernel
// - stripe_kernel_minimum_size: shortest input for stripe_kernel (max: never)

#include <checksum/xxh3.hpp>

// std::array of __m128i or uint64x2_t drops their __may_alias__ attribute, which arrays of accumulators do not need.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

#if defined(CHECKSUM_ACCELERATION) && !CHECKSUM_ACCELERATION
#include <checksum_private/generic/xxh3.hpp>
#elif defined(__x86_64__)
#include <checksum_private/x86_64/xxh3.hpp>
#elif defined(__aarch64__) || defined(__arm__)
#include <checksum_private/arm/xxh3.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/xxh3.hpp>
#else
#include <checksum_private/generic/xxh3.hpp>
#endif

#pragma GCC diagnostic pop

#endif // CHECKSUM_PRIVATE_XXH3_ARCH_HPP
