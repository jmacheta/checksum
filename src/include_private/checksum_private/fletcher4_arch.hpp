#ifndef CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
#define CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP

// CPU kernel of the fletcher4 lane loop, from the directory of the target architecture, selected by the instruction-set
// preset Isa. Each architecture header defines, for a preset Isa:
// - lane_kernel_available<Isa>; if true, lane_kernel<Isa>()
// - lane_kernel_minimum_size<Isa>: shortest input for lane_kernel<Isa>() (max: never)

#include <checksum/fletcher4.hpp>
#include <checksum/isa.hpp>

#include <cstddef>
#include <cstdint>

namespace checksum::fletcher4_detail {

// The lane sums of groups groups of lane_count little-endian words at data.
template <isa Isa> lane_sums lane_kernel(std::byte const *data, std::size_t groups) noexcept;

} // namespace checksum::fletcher4_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/fletcher4.hpp>
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__)
#include <checksum_private/arm/fletcher4.hpp>
#else
#include <checksum_private/generic/fletcher4.hpp>
#endif

#endif // CHECKSUM_PRIVATE_FLETCHER4_ARCH_HPP
