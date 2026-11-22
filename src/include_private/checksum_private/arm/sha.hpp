#ifndef CHECKSUM_PRIVATE_ARM_SHA_HPP
#define CHECKSUM_PRIVATE_ARM_SHA_HPP

// Arm kernels of SHA-1 and SHA-256 (SHA1 and SHA256 instructions) from aarch64_sha2, and of SHA-512 (SHA512 instructions)
// from aarch64_sha512, on little-endian AArch64. MSVC builds the first two only. Included only by sha_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum/sha.hpp>
#include <checksum_private/target.hpp>
#include <checksum_private/vector_array.hpp>

#include <arm_neon.h>

#include <bit>
#include <cstddef>
#include <cstdint>

namespace checksum::sha_detail {

// Where the kernels run: their loads read big-endian words with little-endian lanes.
template <isa Isa, isa Preset> inline constexpr bool arm_sha_target = std::endian::native == std::endian::little && isa_detail::includes(Isa, Preset);

template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha2>)
struct block_kernel<sha_variant::sha1, Isa> {
  static constexpr bool available = true;
  static void compress(word_array<sha_variant::sha1> &words, std::byte const *data, std::size_t blocks) noexcept;
};

template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha2>)
struct block_kernel<sha_variant::sha256, Isa> {
  static constexpr bool available = true;
  static void compress(word_array<sha_variant::sha256> &words, std::byte const *data, std::size_t blocks) noexcept;
};

#if !defined(_MSC_VER) || defined(__clang__)
template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha512>)
struct block_kernel<sha_variant::sha512, Isa> {
  static constexpr bool available = true;
  static void compress(word_array<sha_variant::sha512> &words, std::byte const *data, std::size_t blocks) noexcept;
};
#endif

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): byte views for the vector loads.

// Four rounds per group: the schedule words of the next groups are computed as the rounds consume them.
template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha2>)
void block_kernel<sha_variant::sha1, Isa>::compress(word_array<sha_variant::sha1> &words, std::byte const *data, std::size_t blocks) noexcept {
  uint32x4_t abcd = vld1q_u32(words.data());
  std::uint32_t e = words[4];
  for(; blocks != 0; --blocks, data += 64) {
    uint32x4_t const abcd_before = abcd;
    std::uint32_t const e_before = e;
    auto const *bytes = reinterpret_cast<std::uint8_t const *>(data);
    isa_detail::vector_array<uint32x4_t, 4> messages{
        vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes))), vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 16))),
        vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 32))), vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 48)))};
    for(std::size_t group = 0; group < 20; ++group) {
      uint32x4_t const message = vaddq_u32(messages[group % 4], vdupq_n_u32(sha1_constants[group / 5]));
      std::uint32_t const next_e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
      if(group < 5) {
        abcd = vsha1cq_u32(abcd, e, message);
      } else if(group < 10 || group >= 15) {
        abcd = vsha1pq_u32(abcd, e, message);
      } else {
        abcd = vsha1mq_u32(abcd, e, message);
      }
      e = next_e;
      if(group < 16) {
        messages[group % 4] =
            vsha1su1q_u32(vsha1su0q_u32(messages[group % 4], messages[(group + 1) % 4], messages[(group + 2) % 4]), messages[(group + 3) % 4]);
      }
    }
    abcd = vaddq_u32(abcd, abcd_before);
    e += e_before;
  }
  vst1q_u32(words.data(), abcd);
  words[4] = e;
}

template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha2>)
void block_kernel<sha_variant::sha256, Isa>::compress(word_array<sha_variant::sha256> &words, std::byte const *data, std::size_t blocks) noexcept {
  uint32x4_t abcd = vld1q_u32(words.data());
  uint32x4_t efgh = vld1q_u32(words.data() + 4);
  for(; blocks != 0; --blocks, data += 64) {
    uint32x4_t const abcd_before = abcd;
    uint32x4_t const efgh_before = efgh;
    auto const *bytes = reinterpret_cast<std::uint8_t const *>(data);
    isa_detail::vector_array<uint32x4_t, 4> messages{
        vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes))), vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 16))),
        vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 32))), vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(bytes + 48)))};
    for(std::size_t group = 0; group < 16; ++group) {
      uint32x4_t const message = vaddq_u32(messages[group % 4], vld1q_u32(sha256_constants.data() + (4 * group)));
      if(group < 12) {
        messages[group % 4] =
            vsha256su1q_u32(vsha256su0q_u32(messages[group % 4], messages[(group + 1) % 4]), messages[(group + 2) % 4], messages[(group + 3) % 4]);
      }
      uint32x4_t const abcd_old = abcd;
      abcd = vsha256hq_u32(abcd, efgh, message);
      efgh = vsha256h2q_u32(efgh, abcd_old, message);
    }
    abcd = vaddq_u32(abcd, abcd_before);
    efgh = vaddq_u32(efgh, efgh_before);
  }
  vst1q_u32(words.data(), abcd);
  vst1q_u32(words.data() + 4, efgh);
}

#if !defined(_MSC_VER) || defined(__clang__)
// Two rounds per step, after Arm's example and the Linux sha512-ce code: the state as ab, cd, ef and gh.
template <isa Isa>
  requires(arm_sha_target<Isa, isa::aarch64_sha512>)
void block_kernel<sha_variant::sha512, Isa>::compress(word_array<sha_variant::sha512> &words, std::byte const *data, std::size_t blocks) noexcept {
  uint64x2_t ab = vld1q_u64(words.data());
  uint64x2_t cd = vld1q_u64(words.data() + 2);
  uint64x2_t ef = vld1q_u64(words.data() + 4);
  uint64x2_t gh = vld1q_u64(words.data() + 6);
  for(; blocks != 0; --blocks, data += 128) {
    uint64x2_t const ab_before = ab;
    uint64x2_t const cd_before = cd;
    uint64x2_t const ef_before = ef;
    uint64x2_t const gh_before = gh;
    auto const *bytes = reinterpret_cast<std::uint8_t const *>(data);
    isa_detail::vector_array<uint64x2_t, 8> messages{};
    for(std::size_t index = 0; index < messages.size(); ++index) {
      messages[index] = vreinterpretq_u64_u8(vrev64q_u8(vld1q_u8(bytes + (16 * index))));
    }
    for(std::size_t step = 0; step < 40; ++step) {
      uint64x2_t const added = vaddq_u64(messages[step % 8], vld1q_u64(sha512_constants.data() + (2 * step)));
      if(step < 32) {
        messages[step % 8] = vsha512su1q_u64(vsha512su0q_u64(messages[step % 8], messages[(step + 1) % 8]), messages[(step + 7) % 8],
                                             vextq_u64(messages[(step + 4) % 8], messages[(step + 5) % 8], 1));
      }
      uint64x2_t const sum = vaddq_u64(vextq_u64(added, added, 1), gh);
      uint64x2_t const partial = vsha512hq_u64(sum, vextq_u64(ef, gh, 1), vextq_u64(cd, ef, 1));
      uint64x2_t const new_ef = vaddq_u64(cd, partial);
      uint64x2_t const new_ab = vsha512h2q_u64(partial, cd, ab);
      gh = ef;
      ef = new_ef;
      cd = ab;
      ab = new_ab;
    }
    ab = vaddq_u64(ab, ab_before);
    cd = vaddq_u64(cd, cd_before);
    ef = vaddq_u64(ef, ef_before);
    gh = vaddq_u64(gh, gh_before);
  }
  vst1q_u64(words.data(), ab);
  vst1q_u64(words.data() + 2, cd);
  vst1q_u64(words.data() + 4, ef);
  vst1q_u64(words.data() + 6, gh);
}
#endif

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace checksum::sha_detail

#endif // CHECKSUM_PRIVATE_ARM_SHA_HPP
