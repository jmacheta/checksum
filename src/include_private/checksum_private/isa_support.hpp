#ifndef CHECKSUM_PRIVATE_ISA_SUPPORT_HPP
#define CHECKSUM_PRIVATE_ISA_SUPPORT_HPP

// Run-time CPU support checks, shared by src/isa and the sources that dispatch on an isa_id.

#include <checksum/isa.hpp>

namespace checksum::isa_detail {

// Whether the running CPU supports the x86-64 AVX2 instruction set; false on any other target. __builtin_cpu_supports
// reads the CPU model established at load time, so the check is cheap and keeps no state of its own.
inline bool cpu_supports_x86_avx2() noexcept {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}

} // namespace checksum::isa_detail

#endif // CHECKSUM_PRIVATE_ISA_SUPPORT_HPP
