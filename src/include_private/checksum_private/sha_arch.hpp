#ifndef CHECKSUM_PRIVATE_SHA_ARCH_HPP
#define CHECKSUM_PRIVATE_SHA_ARCH_HPP

// CPU kernels of SHA-1 and SHA-2, from the directory of the target architecture. The primary template of
// block_kernel<Core, Isa> means no kernel; an architecture header specializes it for the cores and presets with one, with:
// - available = true
// - static void compress(word_array<Core> &words, std::byte const *data, std::size_t blocks): blocks whole blocks into words

#include <checksum/isa.hpp>
#include <checksum/sha.hpp>
#include <checksum_private/target.hpp>

#include <cstddef>

// As template arguments (isa_detail::vector_array), __m128i and uint32x4_t drop their __may_alias__ attribute, which kernel
// vectors do not need.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

namespace checksum::sha_detail {

template <sha_variant Core, isa Isa> struct block_kernel {
  static constexpr bool available = false;
};

} // namespace checksum::sha_detail

#if defined(__x86_64__) || defined(_M_X64)
#include <checksum_private/x86_64/sha.hpp>
#elifdef CHECKSUM_ARM64
#include <checksum_private/arm/sha.hpp>
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // CHECKSUM_PRIVATE_SHA_ARCH_HPP
