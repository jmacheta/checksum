#ifndef CHECKSUM_PRIVATE_SHA_LOOPS_HPP
#define CHECKSUM_PRIVATE_SHA_LOOPS_HPP

// The run-time block loops of SHA-1 and SHA-2 for one preset: the CPU kernel of sha_arch.hpp, else the portable rounds of the
// strategy. A source of src/sha/ defines CHECKSUM_PRESET (see target_begin.hpp) and includes this header once, which compiles
// block_loop<Core, Strategy, isa::CHECKSUM_PRESET>() for every core and strategy.

// Every std header of this file and of the architecture headers comes before the target region, so that their inline
// functions compile for the build's own target.
#include <checksum/isa.hpp>
#include <checksum/sha.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <checksum_private/target_begin.hpp>

#include <checksum_private/sha_arch.hpp>

namespace checksum::sha_detail {

// block_loop(), first declared inside the target region: GCC fixes a function's target at its first declaration.
template <sha_variant Core, class Strategy, isa Isa> void block_any(word_array<Core> &words, std::span<std::byte const> data) noexcept;

template <sha_variant Core, class Strategy, isa Isa> void block_any(word_array<Core> &words, std::span<std::byte const> data) noexcept {
  if constexpr(block_kernel<Core, Isa>::available) {
    block_kernel<Core, Isa>::compress(words, data.data(), data.size() / parameters<Core>::block_size);
  } else {
    fold_blocks<Core, Strategy>(words, data);
  }
}

} // namespace checksum::sha_detail

#include <checksum_private/target_end.hpp>

namespace checksum::sha_detail {

template <sha_variant Core, class Strategy, isa Isa> void block_loop(word_array<Core> &words, std::span<std::byte const> data) noexcept {
  block_any<Core, Strategy, Isa>(words, data);
}

template void block_loop<sha_variant::sha1, sha_compact, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha1> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha256, sha_compact, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha256> &,
                                                                                 std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha512, sha_compact, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha512> &,
                                                                                 std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha1, sha_unrolled, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha1> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha256, sha_unrolled, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha256> &,
                                                                                  std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha512, sha_unrolled, isa::CHECKSUM_PRESET>(word_array<sha_variant::sha512> &,
                                                                                  std::span<std::byte const>) noexcept;

// The dispatch engines choose exactly the presets this algorithm has sources for.
static_assert(own_loops.contains(isa::CHECKSUM_PRESET));

} // namespace checksum::sha_detail

#endif // CHECKSUM_PRIVATE_SHA_LOOPS_HPP
