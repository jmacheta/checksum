#ifndef CHECKSUM_PRIVATE_TARGET_HPP
#define CHECKSUM_PRIVATE_TARGET_HPP

// The shape of the target that the loops and kernels adapt to; the private headers test these names and CHECKSUM_ARM64
// instead of the architecture macros of each compiler. No std header: the loops headers include it in the target region.

#include <checksum/isa.hpp>
#include <checksum_private/presets.hpp>

// AArch64 as GCC and Clang (__aarch64__) or MSVC (_M_ARM64) name it.
#if defined(__aarch64__) || defined(_M_ARM64)
#define CHECKSUM_ARM64
#endif

namespace checksum::target_detail {

#if defined(CHECKSUM_ARM64)
inline constexpr bool arm64_target = true;
#else
inline constexpr bool arm64_target = false;
#endif

#if defined(__arm__)
inline constexpr bool arm32_target = true;
#else
inline constexpr bool arm32_target = false;
#endif

// Pointers narrower than 64 bits.
inline constexpr bool word32_target = sizeof(void *) < 8;

// 32-bit Arm below thumb2_neon: Cortex-M and Cortex-R cores, which run in order.
template <isa Isa> inline constexpr bool in_order_target = arm32_target && !isa_detail::includes(Isa, isa::thumb2_neon);

// The constraint of a kernel specialization that every preset of the architecture runs.
template <isa Isa> inline constexpr bool any_preset = true;

} // namespace checksum::target_detail

#endif // CHECKSUM_PRIVATE_TARGET_HPP
