#ifndef CHECKSUM_PRIVATE_ARM_TARGET_HPP
#define CHECKSUM_PRIVATE_ARM_TARGET_HPP

// AArch64 as GCC and Clang (__aarch64__) or MSVC (_M_ARM64) name it: the Arm headers test only CHECKSUM_ARM64 and arm64_target.

#if defined(__aarch64__) || defined(_M_ARM64)
#define CHECKSUM_ARM64
#endif

namespace checksum::arm_detail {

#if defined(CHECKSUM_ARM64)
inline constexpr bool arm64_target = true;
#else
inline constexpr bool arm64_target = false;
#endif

} // namespace checksum::arm_detail

#endif // CHECKSUM_PRIVATE_ARM_TARGET_HPP
