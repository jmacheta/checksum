#ifndef CHECKSUM_PRIVATE_XXH3_ARCH_HPP
#define CHECKSUM_PRIVATE_XXH3_ARCH_HPP

// CPU kernels of the XXH3 stripe loop, from the directory of the target architecture, selected by the instruction-set preset Isa.
// Each architecture header defines, for a preset Isa:
// - stripe_kernel_available<Isa>; if true, stripe_kernel<Isa> with the members of portable_kernel
// - stripe_kernel_minimum_size<Isa>: shortest input for stripe_kernel<Isa>() (max: never)

#include <checksum/xxh3.hpp>
#include <checksum/isa.hpp>

// std::array of __m128i or uint64x2_t drops their __may_alias__ attribute, which arrays of accumulators do not need.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

namespace checksum::xxh3_detail {

template <isa Isa> struct stripe_kernel;

} // namespace checksum::xxh3_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/xxh3.hpp>
#elif defined(__aarch64__) || defined(__arm__)
#include <checksum_private/arm/xxh3.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/xxh3.hpp>
#else
#include <checksum_private/generic/xxh3.hpp>
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // CHECKSUM_PRIVATE_XXH3_ARCH_HPP
