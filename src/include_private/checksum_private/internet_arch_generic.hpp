#ifndef CHECKSUM_PRIVATE_INTERNET_ARCH_GENERIC_HPP
#define CHECKSUM_PRIVATE_INTERNET_ARCH_GENERIC_HPP

// No kernel: the portable loop sums everything. Included only by internet_arch.hpp.

#include <cstddef>
#include <limits>

namespace checksum::internet_detail {

inline constexpr bool vector_sum_available = false;

inline constexpr std::size_t vector_sum_minimum_size = std::numeric_limits<std::size_t>::max();

} // namespace checksum::internet_detail

#endif // CHECKSUM_PRIVATE_INTERNET_ARCH_GENERIC_HPP
