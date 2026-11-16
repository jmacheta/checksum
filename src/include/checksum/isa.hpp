#ifndef CHECKSUM_ISA_HPP
#define CHECKSUM_ISA_HPP

#include <concepts>
#include <cstdint>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_isa Instruction sets
 *   Presets of instruction sets that the strategies of the algorithms with CPU kernels can name; each one selects the
 *   variant of the kernel compiled with that instruction set, whatever the build's compiler flags.
 *   @{
 */

namespace checksum {

/// x86-64 with the AVX2 instruction set: 256-bit vectors, no FMA.
struct isa_x86_avx2 {};

/// The instruction sets a strategy can name. Each has its variants compiled in only on architectures and in builds
/// that support it; using one that was not compiled in is a compile error.
template <class Isa>
concept isa = std::same_as<Isa, isa_x86_avx2>;

/// Which CPU variant to run, chosen at run time: none the loop the build's compiler flags select, the others the loop of
/// the matching preset. Asking at run time for a variant the build did not compile, or the CPU does not support, falls
/// back to none; naming such a preset in a strategy is a compile error instead.
enum class isa_id : std::uint8_t { none, x86_avx2 };

/// The best variant that this build compiled a kernel for and the running CPU supports; isa_id::none if there is none.
/// Detection runs per call and keeps no state; a program that never calls it links no detection code.
[[nodiscard]] isa_id detect_isa() noexcept;

} // namespace checksum

///@}
///@}

#endif // CHECKSUM_ISA_HPP
