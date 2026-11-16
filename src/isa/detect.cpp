// Run-time CPU detection: the best instruction-set variant this build compiled a kernel for and this CPU can run.
// The library links this source only into programs that call detect_isa().

#include <checksum/isa.hpp>
#include <checksum_private/isa_support.hpp>

namespace checksum {

isa_id detect_isa() noexcept {
#ifdef CHECKSUM_INTERNET_X86_AVX2
  if(isa_detail::cpu_supports_x86_avx2()) {
    return isa_id::x86_avx2;
  }
#endif
  return isa_id::none;
}

} // namespace checksum
