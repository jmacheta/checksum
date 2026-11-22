#ifndef CHECKSUM_PRIVATE_X86_64_SHA_HPP
#define CHECKSUM_PRIVATE_X86_64_SHA_HPP

// SHA-NI kernels of SHA-1 and SHA-256 for the x86_sha preset, after Intel's "New Instructions Supporting the Secure Hash
// Algorithm on Intel Architecture Processors". SHA-512 has no such instructions here. Included only by sha_arch.hpp.

#include <checksum/isa.hpp>
#include <checksum/sha.hpp>
#include <checksum_private/vector_array.hpp>

#include <immintrin.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace checksum::sha_detail {

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): unaligned vector loads and stores, the intrinsics' interface.

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
struct block_kernel<sha_variant::sha1, Isa> {
  static constexpr bool available = true;
  static void compress(word_array<sha_variant::sha1> &words, std::byte const *data, std::size_t blocks) noexcept;
  // Rounds 4 * Group .. 4 * Group + 3; messages holds the schedule words of groups Group .. Group + 3 as far as computed.
  // Members, not lambdas: GCC compiles lambdas without the preset's target.
  template <int Group>
  static void group(__m128i &abcd, __m128i &e0, __m128i &e1, isa_detail::vector_array<__m128i, 4> &messages, std::byte const *block) noexcept;
  template <int... Group>
  static void groups(__m128i &abcd, __m128i &e0, __m128i &e1, isa_detail::vector_array<__m128i, 4> &messages, std::byte const *block,
                     std::integer_sequence<int, Group...> sequence) noexcept;
};

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
struct block_kernel<sha_variant::sha256, Isa> {
  static constexpr bool available = true;
  static void compress(word_array<sha_variant::sha256> &words, std::byte const *data, std::size_t blocks) noexcept;
  // Rounds 4 * Group .. 4 * Group + 3, as for SHA-1; the state as ABEF and CDGH.
  template <int Group>
  static void group(__m128i &abef, __m128i &cdgh, isa_detail::vector_array<__m128i, 4> &messages, std::byte const *block) noexcept;
  template <int... Group>
  static void groups(__m128i &abef, __m128i &cdgh, isa_detail::vector_array<__m128i, 4> &messages, std::byte const *block,
                     std::integer_sequence<int, Group...> sequence) noexcept;
};

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
template <int Group>
CHECKSUM_ALWAYS_INLINE inline void block_kernel<sha_variant::sha1, Isa>::group(__m128i &abcd, __m128i &e0, __m128i &e1,
                                                                               isa_detail::vector_array<__m128i, 4> &messages,
                                                                               std::byte const *block) noexcept {
  if constexpr(Group < 4) {
    // The 16 bytes reversed: the first word in the high lane, big-endian.
    __m128i const reverse = _mm_set_epi64x(0x0001020304050607, 0x08090A0B0C0D0E0F);
    messages[Group] = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<__m128i const *>(block + (16 * Group))), reverse);
  }
  __m128i &current = Group % 2 == 0 ? e0 : e1;
  __m128i &next = Group % 2 == 0 ? e1 : e0;
  if constexpr(Group == 0) {
    current = _mm_add_epi32(current, messages[0]);
  } else {
    current = _mm_sha1nexte_epu32(current, messages[Group % 4]);
  }
  next = abcd;
  abcd = _mm_sha1rnds4_epu32(abcd, current, Group / 5);
  // The schedule: group g + 4 is msg2(msg1(g, g + 1) ^ (g + 2), g + 3), its steps spread over the groups before it.
  if constexpr(Group >= 3 && Group < 19) {
    messages[(Group + 1) % 4] = _mm_sha1msg2_epu32(messages[(Group + 1) % 4], messages[Group % 4]);
  }
  if constexpr(Group >= 1 && Group < 17) {
    messages[(Group + 3) % 4] = _mm_sha1msg1_epu32(messages[(Group + 3) % 4], messages[Group % 4]);
  }
  if constexpr(Group >= 2 && Group < 18) {
    messages[(Group + 2) % 4] = _mm_xor_si128(messages[(Group + 2) % 4], messages[Group % 4]);
  }
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
template <int... Group>
CHECKSUM_ALWAYS_INLINE inline void
block_kernel<sha_variant::sha1, Isa>::groups(__m128i &abcd, __m128i &e0, __m128i &e1, isa_detail::vector_array<__m128i, 4> &messages,
                                             std::byte const *block, std::integer_sequence<int, Group...> /*sequence*/) noexcept {
  (group<Group>(abcd, e0, e1, messages, block), ...);
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
void block_kernel<sha_variant::sha1, Isa>::compress(word_array<sha_variant::sha1> &words, std::byte const *data, std::size_t blocks) noexcept {
  // A in the high lane of abcd, E in the high lane of e0.
  __m128i abcd = _mm_shuffle_epi32(_mm_loadu_si128(reinterpret_cast<__m128i const *>(words.data())), 0x1B);
  __m128i e0 = _mm_set_epi32(static_cast<int>(words[4]), 0, 0, 0);
  for(; blocks != 0; --blocks, data += 64) {
    __m128i const abcd_before = abcd;
    __m128i const e_before = e0;
    __m128i e1{};
    isa_detail::vector_array<__m128i, 4> messages{};
    groups(abcd, e0, e1, messages, data, std::make_integer_sequence<int, 20>{});
    e0 = _mm_sha1nexte_epu32(e0, e_before);
    abcd = _mm_add_epi32(abcd, abcd_before);
  }
  _mm_storeu_si128(reinterpret_cast<__m128i *>(words.data()), _mm_shuffle_epi32(abcd, 0x1B));
  words[4] = static_cast<std::uint32_t>(_mm_extract_epi32(e0, 3));
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
template <int Group>
CHECKSUM_ALWAYS_INLINE inline void block_kernel<sha_variant::sha256, Isa>::group(__m128i &abef, __m128i &cdgh,
                                                                                 isa_detail::vector_array<__m128i, 4> &messages,
                                                                                 std::byte const *block) noexcept {
  if constexpr(Group < 4) {
    // Each 32-bit word byte-reversed: big-endian words in their own lanes.
    __m128i const reverse = _mm_set_epi64x(0x0C0D0E0F08090A0B, 0x0405060700010203);
    messages[Group] = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<__m128i const *>(block + (16 * Group))), reverse);
  }
  __m128i const message =
      _mm_add_epi32(messages[Group % 4], _mm_loadu_si128(reinterpret_cast<__m128i const *>(sha256_constants.data() + (4 * Group))));
  cdgh = _mm_sha256rnds2_epu32(cdgh, abef, message);
  // The schedule: group g + 4 is msg2(msg1(g, g + 1) + words 4g + 9 .. 4g + 12, g + 3).
  if constexpr(Group >= 3 && Group < 15) {
    __m128i const middle = _mm_alignr_epi8(messages[Group % 4], messages[(Group + 3) % 4], 4);
    messages[(Group + 1) % 4] = _mm_sha256msg2_epu32(_mm_add_epi32(messages[(Group + 1) % 4], middle), messages[Group % 4]);
  }
  abef = _mm_sha256rnds2_epu32(abef, cdgh, _mm_shuffle_epi32(message, 0x0E));
  if constexpr(Group >= 1 && Group < 13) {
    messages[(Group + 3) % 4] = _mm_sha256msg1_epu32(messages[(Group + 3) % 4], messages[Group % 4]);
  }
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
template <int... Group>
CHECKSUM_ALWAYS_INLINE inline void
block_kernel<sha_variant::sha256, Isa>::groups(__m128i &abef, __m128i &cdgh, isa_detail::vector_array<__m128i, 4> &messages, std::byte const *block,
                                               std::integer_sequence<int, Group...> /*sequence*/) noexcept {
  (group<Group>(abef, cdgh, messages, block), ...);
}

template <isa Isa>
  requires(isa_detail::includes(Isa, isa::x86_sha))
void block_kernel<sha_variant::sha256, Isa>::compress(word_array<sha_variant::sha256> &words, std::byte const *data, std::size_t blocks) noexcept {
  // From the words A .. H to the lanes ABEF and CDGH that sha256rnds2 takes, highest lane first.
  __m128i const dcba = _mm_loadu_si128(reinterpret_cast<__m128i const *>(words.data()));
  __m128i const hgfe = _mm_loadu_si128(reinterpret_cast<__m128i const *>(words.data() + 4));
  __m128i const cdab = _mm_shuffle_epi32(dcba, 0xB1);
  __m128i const efgh = _mm_shuffle_epi32(hgfe, 0x1B);
  __m128i abef = _mm_alignr_epi8(cdab, efgh, 8);
  __m128i cdgh = _mm_blend_epi16(efgh, cdab, 0xF0);
  for(; blocks != 0; --blocks, data += 64) {
    __m128i const abef_before = abef;
    __m128i const cdgh_before = cdgh;
    isa_detail::vector_array<__m128i, 4> messages{};
    groups(abef, cdgh, messages, data, std::make_integer_sequence<int, 16>{});
    abef = _mm_add_epi32(abef, abef_before);
    cdgh = _mm_add_epi32(cdgh, cdgh_before);
  }
  __m128i const feba = _mm_shuffle_epi32(abef, 0x1B);
  __m128i const dchg = _mm_shuffle_epi32(cdgh, 0xB1);
  _mm_storeu_si128(reinterpret_cast<__m128i *>(words.data()), _mm_blend_epi16(feba, dchg, 0xF0));
  _mm_storeu_si128(reinterpret_cast<__m128i *>(words.data() + 4), _mm_alignr_epi8(dchg, feba, 8));
}

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace checksum::sha_detail

#endif // CHECKSUM_PRIVATE_X86_64_SHA_HPP
