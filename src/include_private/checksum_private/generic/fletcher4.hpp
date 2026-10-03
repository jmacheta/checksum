#ifndef CHECKSUM_PRIVATE_GENERIC_FLETCHER4_HPP
#define CHECKSUM_PRIVATE_GENERIC_FLETCHER4_HPP

// No kernel: the portable lanes sum everything. Included only by fletcher4_arch.hpp.

#include <cstddef>
#include <limits>

namespace checksum::fletcher4_detail {

inline constexpr bool lane_kernel_available = false;

inline constexpr std::size_t lane_kernel_minimum_size = std::numeric_limits<std::size_t>::max();

} // namespace checksum::fletcher4_detail

#endif // CHECKSUM_PRIVATE_GENERIC_FLETCHER4_HPP
