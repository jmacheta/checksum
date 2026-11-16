// Run-time loop of the Internet checksum compiled for x86-64 AVX2: the build adds -mavx2 to this source only, so
// internet_arch.hpp selects the AVX2 kernel here whatever the build's flags. Built only where that is possible; the
// build then defines CHECKSUM_INTERNET_X86_AVX2.

#include <checksum_private/internet_loops.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::internet_detail {

std::uint16_t sum_loop_x86_avx2(std::span<std::byte const> data) noexcept { return sum_any(data); }

} // namespace checksum::internet_detail
