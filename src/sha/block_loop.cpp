// The run-time block loops of SHA-1 and SHA-2 for both strategies, built only for isa::generic: no preset has a kernel yet, so
// every preset runs these.

#include <checksum/sha.hpp>

#include <span>

namespace checksum::sha_detail {

template <sha_variant Core, class Strategy> void block_loop(word_array<Core> &words, std::span<std::byte const> data) noexcept {
  fold_blocks<Core, Strategy>(words, data);
}

template void block_loop<sha_variant::sha1, sha_compact>(word_array<sha_variant::sha1> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha256, sha_compact>(word_array<sha_variant::sha256> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha512, sha_compact>(word_array<sha_variant::sha512> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha1, sha_unrolled>(word_array<sha_variant::sha1> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha256, sha_unrolled>(word_array<sha_variant::sha256> &, std::span<std::byte const>) noexcept;
template void block_loop<sha_variant::sha512, sha_unrolled>(word_array<sha_variant::sha512> &, std::span<std::byte const>) noexcept;

} // namespace checksum::sha_detail
