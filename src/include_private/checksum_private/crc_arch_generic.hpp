#ifndef CHECKSUM_PRIVATE_CRC_ARCH_GENERIC_HPP
#define CHECKSUM_PRIVATE_CRC_ARCH_GENERIC_HPP

// No kernel: the table strategies run their portable loops. Included only by crc_arch.hpp.

#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::crc_detail {

inline constexpr bool folding_available = false;

template <std::uint32_t Polynomial> inline constexpr bool crc32_instructions_available = false;

inline constexpr std::size_t wide_folding_minimum_size = std::numeric_limits<std::size_t>::max();

inline constexpr std::size_t crc32_folding_minimum_size = std::numeric_limits<std::size_t>::max();

} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_CRC_ARCH_GENERIC_HPP
