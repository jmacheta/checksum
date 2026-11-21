#ifndef CHECKSUM_PRIVATE_XXH3_ARCH_HPP
#define CHECKSUM_PRIVATE_XXH3_ARCH_HPP

// CPU kernels of the XXH3 stripe loop, from the directory of the target architecture. The primary template of
// stripe_kernel<Isa> means no kernel; an architecture header specializes it for the presets with one, with:
// - available = true and the members of portable_kernel, defined CHECKSUM_ALWAYS_INLINE: the stripe loop calls them per stripe
// - minimum_size: shortest input for the kernel

#include <checksum/isa.hpp>
#include <checksum/xxh3.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <limits>

// As template arguments (std::conditional_t, isa_detail::vector_array), __m128i, __m256i and uint64x2_t drop their __may_alias__
// attribute, which kernel vectors do not need.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

namespace checksum::xxh3_detail {

template <isa Isa> struct stripe_kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

} // namespace checksum::xxh3_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/xxh3.hpp>
#elif defined(CHECKSUM_ARM64) || defined(__arm__)
#include <checksum_private/arm/xxh3.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/xxh3.hpp>
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // CHECKSUM_PRIVATE_XXH3_ARCH_HPP
