// Run-time selection of the Internet checksum loop: an isa_id picks one of the loops this build compiled. A variant the
// build did not compile, or the running CPU does not support, runs the loop of internet_loop instead, so no call runs
// instructions the CPU lacks beyond those the build's own flags already require. Linked only into programs that pass
// an isa_id.

#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <checksum_private/isa_support.hpp>

#include <cstddef>
#include <span>

namespace checksum {

internet_state internet_update(internet_state state, std::span<std::byte const> data, [[maybe_unused]] isa_id isa) noexcept {
#ifdef CHECKSUM_INTERNET_X86_AVX2
  if(isa == isa_id::x86_avx2 && isa_detail::cpu_supports_x86_avx2()) {
    return internet_detail::combine(state, internet_detail::sum_loop_x86_avx2(data), data.size());
  }
#endif
  return internet_detail::combine(state, internet_detail::sum_loop(data), data.size());
}

} // namespace checksum
