#ifndef CHECKSUM_PRIVATE_GENERIC_XXH3_HPP
#define CHECKSUM_PRIVATE_GENERIC_XXH3_HPP

// No kernel: the portable kernel folds everything. Included only by xxh3_arch.hpp.

#include <cstddef>
#include <limits>

namespace checksum::xxh3_detail {

inline constexpr bool stripe_kernel_available = false;

inline constexpr std::size_t stripe_kernel_minimum_size = std::numeric_limits<std::size_t>::max();

// Named by the branch that stripe_kernel_available disables.
using stripe_kernel = portable_kernel;

} // namespace checksum::xxh3_detail

#endif // CHECKSUM_PRIVATE_GENERIC_XXH3_HPP
