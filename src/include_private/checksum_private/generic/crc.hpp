#ifndef CHECKSUM_PRIVATE_GENERIC_CRC_HPP
#define CHECKSUM_PRIVATE_GENERIC_CRC_HPP

// No kernel for any preset: the table strategies run their portable loops. Included only by crc_arch.hpp.

#include <checksum/isa.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace checksum::crc_detail {
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header): a private header, included only by src/crc/*.cpp.
namespace {

template <isa Isa> inline constexpr bool folding_available = false;

template <isa Isa, std::uint32_t Polynomial> inline constexpr bool crc32_instructions_available = false;

template <isa Isa> inline constexpr std::size_t wide_folding_minimum_size = std::numeric_limits<std::size_t>::max();

inline constexpr std::size_t crc32_folding_minimum_size = std::numeric_limits<std::size_t>::max();

} // namespace
} // namespace checksum::crc_detail

#endif // CHECKSUM_PRIVATE_GENERIC_CRC_HPP
