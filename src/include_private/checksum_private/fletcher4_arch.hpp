#ifndef CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP

// CPU kernel of the fletcher4 lane loop, from the directory of the target architecture; without the instruction-set extensions, or
// with CHECKSUM_ACCELERATION defined to 0, the portable lanes sum everything. Each architecture header defines:
// - lane_kernel_available; if true, lane_kernel()
// - lane_kernel_minimum_size: shortest input for lane_kernel() (max: never)

#include <array>
#include <cstddef>
#include <cstdint>

namespace checksum::fletcher4_detail {

// Interleaved lanes: lane j folds the words j, j + 4, j + 8, ... as a message of its own.
inline constexpr std::size_t lane_count = 4;

// lane_sums[source][lane], starting at 0: the sum1 to sum4 of each lane, so that one sum of all lanes is contiguous.
using lane_sums = std::array<std::array<std::uint64_t, lane_count>, 4>;

// The lane sums of groups groups of lane_count little-endian words at data.
inline lane_sums lane_kernel(std::byte const *data, std::size_t groups) noexcept;

} // namespace checksum::fletcher4_detail

#if defined(CHECKSUM_ACCELERATION) && !CHECKSUM_ACCELERATION
#include <checksum_private/generic/fletcher4.hpp>
#elif defined(__x86_64__)
#include <checksum_private/x86_64/fletcher4.hpp>
#elif defined(__aarch64__) || defined(__arm__)
#include <checksum_private/arm/fletcher4.hpp>
#else
#include <checksum_private/generic/fletcher4.hpp>
#endif

#endif // CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
