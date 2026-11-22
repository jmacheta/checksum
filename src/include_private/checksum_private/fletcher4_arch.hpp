#ifndef CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP

// CPU kernel of the fletcher4 lane loop, from the directory of the target architecture. The primary template of
// lane_kernel<Isa> means no kernel; an architecture header specializes it for the presets with one, with:
// - available = true; static lane_sums sum(std::byte const *data, std::size_t groups): the lane sums of groups groups of
//   lane_count little-endian words at data
// - minimum_size: shortest input for the kernel

#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::fletcher4_detail {

template <isa Isa> struct lane_kernel {
  static constexpr bool available = false;
  static constexpr std::size_t minimum_size = std::numeric_limits<std::size_t>::max();
};

} // namespace checksum::fletcher4_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/fletcher4.hpp>
#elif defined(CHECKSUM_ARM64) || defined(__arm__)
#include <checksum_private/arm/fletcher4.hpp>
#elif defined(__riscv) && __riscv_xlen == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <checksum_private/riscv64/fletcher4.hpp>
#endif

#endif // CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
