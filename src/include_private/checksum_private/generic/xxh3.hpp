#ifndef CHECKSUM_PRIVATE_GENERIC_XXH3_HPP
#define CHECKSUM_PRIVATE_GENERIC_XXH3_HPP

// No kernel: the portable kernel folds everything. Included only by xxh3_arch.hpp.

#include <checksum/isa.hpp>

#include <cstddef>
#include <limits>

namespace checksum::xxh3_detail {

template <isa Isa> inline constexpr bool stripe_kernel_available = false;

template <isa Isa> inline constexpr std::size_t stripe_kernel_minimum_size = std::numeric_limits<std::size_t>::max();

} // namespace checksum::xxh3_detail

#endif // CHECKSUM_PRIVATE_GENERIC_XXH3_HPP
