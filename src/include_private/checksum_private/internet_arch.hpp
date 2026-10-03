#ifndef CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
#define CHECKSUM_PRIVATE_INTERNET_ARCH_HPP

// CPU kernel of the Internet checksum, chosen by the compiler's predefined instruction-set macros; with none of them, or
// CHECKSUM_ACCELERATION defined to 0, the portable loop sums everything. Each architecture header defines:
// - vector_sum_available; if true, vector_sum()
// - vector_sum_minimum_size: shortest input for vector_sum() (max: never)

#include <checksum/internet.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

// Sums the leading blocks of data as native-order words, unfolded, and drops them from data.
inline std::uint64_t vector_sum(std::span<std::byte const> &data) noexcept;

} // namespace checksum::internet_detail

#if defined(CHECKSUM_ACCELERATION) && !CHECKSUM_ACCELERATION
#include <checksum_private/internet_arch_generic.hpp>
#elif defined(__x86_64__) && defined(__AVX2__)
#include <checksum_private/internet_arch_x86_64.hpp>
#else
#include <checksum_private/internet_arch_generic.hpp>
#endif

#endif // CHECKSUM_PRIVATE_INTERNET_ARCH_HPP
