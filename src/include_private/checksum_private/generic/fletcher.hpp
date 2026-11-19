#ifndef CHECKSUM_PRIVATE_GENERIC_FLETCHER_HPP
#define CHECKSUM_PRIVATE_GENERIC_FLETCHER_HPP

// No kernel for values of Bits bits and preset Isa: the portable loop sums everything. Every architecture header includes
// it and specializes kernel and group_kernel for the value sizes and presets it accelerates.

#include <checksum/isa.hpp>

#include <cstddef>
#include <limits>

namespace checksum::fletcher_detail {

template <unsigned Bits, isa Isa> struct kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

// A second kernel with the interface of kernel, for long inputs only: from its minimum_size it runs instead of kernel.
template <unsigned Bits, isa Isa> struct group_kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

} // namespace checksum::fletcher_detail

#endif // CHECKSUM_PRIVATE_GENERIC_FLETCHER_HPP
