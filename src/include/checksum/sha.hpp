#ifndef CHECKSUM_SHA_HPP
#define CHECKSUM_SHA_HPP

#include <checksum/byte_range.hpp>
#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 *   @defgroup checksum_sha SHA-1 and SHA-2
 *   The hashes of FIPS 180-4: SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, SHA-512/224 and SHA-512/256, at compile time
 *   or at run time.
 *   @{
 */

namespace checksum {

/// A hash of FIPS 180-4. SHA-1 is broken for collisions: use it only where a format requires it.
enum class sha_variant : std::uint8_t {
  sha1,       ///< SHA-1, 20-byte digest.
  sha224,     ///< SHA-224, 28-byte digest.
  sha256,     ///< SHA-256, 32-byte digest.
  sha384,     ///< SHA-384, 48-byte digest.
  sha512,     ///< SHA-512, 64-byte digest.
  sha512_224, ///< SHA-512/224, 28-byte digest.
  sha512_256, ///< SHA-512/256, 32-byte digest.
};

} // namespace checksum

namespace checksum::sha_detail {

// The variant whose compression function Variant runs: sha1, sha256 for SHA-224 and SHA-256, sha512 for the others.
constexpr sha_variant core(sha_variant variant) noexcept;

// The word, block and digest sizes and the initial hash value of each variant.
template <sha_variant Variant> struct parameters;

} // namespace checksum::sha_detail

namespace checksum {

/// Strategy of sha_engine, the default: one round per iteration and a 16-word message schedule, the least code and stack.
struct sha_compact {};

/// Strategy of sha_engine: eight rounds per iteration (five for SHA-1) and the whole message schedule, faster on most cores
/// for more code and stack; on 32-bit in-order cores SHA-512 is slower.
struct sha_unrolled {};

/// sha_compact or sha_unrolled.
template <class Strategy>
concept sha_strategy = std::same_as<Strategy, sha_compact> || std::same_as<Strategy, sha_unrolled>;

/// Running hash of Variant, shared by both strategies: the hash value after the whole blocks, the byte count and the unfinished block. Every value is
/// valid; the default is the empty message.
template <sha_variant Variant> struct sha_state {
  /// The hash value after the whole blocks folded so far.
  std::remove_const_t<decltype(sha_detail::parameters<Variant>::initial)> words = sha_detail::parameters<Variant>::initial;
  std::uint64_t length = 0;                                                    ///< Bytes folded so far.
  std::array<std::byte, sha_detail::parameters<Variant>::block_size> buffer{}; ///< The unfinished block: its first length % buffer.size() bytes.

  /// Equal if they hash alike: the same words, length and unfinished block.
  friend constexpr bool operator==(sha_state const &left, sha_state const &right) noexcept {
    auto const buffered = static_cast<std::size_t>(left.length % left.buffer.size());
    return left.words == right.words && left.length == right.length &&
           std::ranges::equal(std::span(left.buffer).first(buffered), std::span(right.buffer).first(buffered));
  }
};

/// The hash Variant, computed as Strategy chooses (sha_compact or sha_unrolled). Every instruction-set preset Isa
/// (checksum/isa.hpp) runs the loop of isa::generic. Empty: make one wherever it is needed.
template <sha_variant Variant, sha_strategy Strategy = sha_compact, isa Isa = isa::generic>
  requires(isa_available(Isa))
class sha_engine {
public:
  using state_type = sha_state<Variant>; ///< Running hash.
  /// The digest, in the byte order of FIPS 180-4.
  using value_type = std::array<std::uint8_t, sha_detail::parameters<Variant>::digest_size>;

  /// Isa, whose loop is generic's.
  [[nodiscard]] static constexpr isa preset() noexcept;

  /// State of an empty message.
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// Folds data into state; data may be split anywhere. Messages may be up to 2^61 - 1 bytes long.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// The digest of the message folded into state.
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// finalize(update(initial(), data)).
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// finalize(update(initial(), data)) for a byte range.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;
};

/// sha_engine<Variant, Strategy, Isa>{}.compute(data).
template <sha_variant Variant, sha_strategy Strategy = sha_compact, isa Isa = isa::generic, byte_range Range>
[[nodiscard]] constexpr sha_engine<Variant, Strategy, Isa>::value_type sha_compute(Range &&data) noexcept;

} // namespace checksum

namespace checksum::sha_detail {

// The presets a dispatch engine chooses from: generic alone, whose loop every preset runs.
inline constexpr isa_set own_loops = isa_set{};

} // namespace checksum::sha_detail

namespace checksum::detail {

template <sha_variant Variant, class Strategy, isa Old, isa Isa> struct rebind_preset<sha_engine<Variant, Strategy, Old>, Isa> {
  using type = sha_engine<Variant, Strategy, Isa>;
};

} // namespace checksum::detail

namespace checksum {

/// A sha_engine with the dispatch engines' interface. No preset has a loop of its own yet, so it always chooses generic.
template <sha_variant Variant, sha_strategy Strategy = sha_compact>
class sha_dispatch_engine : public detail::dispatch_engine<sha_engine<Variant, Strategy>, sha_detail::own_loops> {
public:
  /// cpu does not change the loop.
  constexpr explicit sha_dispatch_engine(isa_set cpu = detect_isa()) noexcept;
};

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::sha_detail {

template <sha_variant Variant> using word = parameters<Variant>::word;

template <sha_variant Variant> using word_array = std::remove_const_t<decltype(parameters<Variant>::initial)>;

inline constexpr std::array<std::uint32_t, 4> sha1_constants{0x5A827999U, 0x6ED9EBA1U, 0x8F1BBCDCU, 0xCA62C1D6U};

// The fractional parts of the cube roots of the first 64 and 80 primes.
inline constexpr std::array<std::uint32_t, 64> sha256_constants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU,
    0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU,
    0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U,
    0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU,
    0x682E6FF3U, 0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U};

inline constexpr std::array<std::uint64_t, 80> sha512_constants{
    0x428A2F98D728AE22U, 0x7137449123EF65CDU, 0xB5C0FBCFEC4D3B2FU, 0xE9B5DBA58189DBBCU, 0x3956C25BF348B538U, 0x59F111F1B605D019U, 0x923F82A4AF194F9BU,
    0xAB1C5ED5DA6D8118U, 0xD807AA98A3030242U, 0x12835B0145706FBEU, 0x243185BE4EE4B28CU, 0x550C7DC3D5FFB4E2U, 0x72BE5D74F27B896FU, 0x80DEB1FE3B1696B1U,
    0x9BDC06A725C71235U, 0xC19BF174CF692694U, 0xE49B69C19EF14AD2U, 0xEFBE4786384F25E3U, 0x0FC19DC68B8CD5B5U, 0x240CA1CC77AC9C65U, 0x2DE92C6F592B0275U,
    0x4A7484AA6EA6E483U, 0x5CB0A9DCBD41FBD4U, 0x76F988DA831153B5U, 0x983E5152EE66DFABU, 0xA831C66D2DB43210U, 0xB00327C898FB213FU, 0xBF597FC7BEEF0EE4U,
    0xC6E00BF33DA88FC2U, 0xD5A79147930AA725U, 0x06CA6351E003826FU, 0x142929670A0E6E70U, 0x27B70A8546D22FFCU, 0x2E1B21385C26C926U, 0x4D2C6DFC5AC42AEDU,
    0x53380D139D95B3DFU, 0x650A73548BAF63DEU, 0x766A0ABB3C77B2A8U, 0x81C2C92E47EDAEE6U, 0x92722C851482353BU, 0xA2BFE8A14CF10364U, 0xA81A664BBC423001U,
    0xC24B8B70D0F89791U, 0xC76C51A30654BE30U, 0xD192E819D6EF5218U, 0xD69906245565A910U, 0xF40E35855771202AU, 0x106AA07032BBD1B8U, 0x19A4C116B8D2D0C8U,
    0x1E376C085141AB53U, 0x2748774CDF8EEB99U, 0x34B0BCB5E19B48A8U, 0x391C0CB3C5C95A63U, 0x4ED8AA4AE3418ACBU, 0x5B9CCA4F7763E373U, 0x682E6FF3D6B2B8A3U,
    0x748F82EE5DEFB2FCU, 0x78A5636F43172F60U, 0x84C87814A1F0AB72U, 0x8CC702081A6439ECU, 0x90BEFFFA23631E28U, 0xA4506CEBDE82BDE9U, 0xBEF9A3F7B2C67915U,
    0xC67178F2E372532BU, 0xCA273ECEEA26619CU, 0xD186B8C721C0C207U, 0xEADA7DD6CDE0EB1EU, 0xF57D4F7FEE6ED178U, 0x06F067AA72176FBAU, 0x0A637DC5A2C898A6U,
    0x113F9804BEF90DAEU, 0x1B710B35131C471BU, 0x28DB77F523047D84U, 0x32CAAB7B40C72493U, 0x3C9EBE0A15C9BEBCU, 0x431D67C49C100D4CU, 0x4CC5D4BECB3E42B6U,
    0x597F299CFC657E2AU, 0x5FCB6FAB3AD6FAECU, 0x6C44198C4A475817U};

template <> struct parameters<sha_variant::sha1> {
  using word = std::uint32_t;
  static constexpr std::size_t block_size = 64;
  static constexpr std::size_t digest_size = 20;
  static constexpr std::array<word, 5> initial{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U};
};

template <> struct parameters<sha_variant::sha224> {
  using word = std::uint32_t;
  static constexpr std::size_t block_size = 64;
  static constexpr std::size_t digest_size = 28;
  static constexpr std::array<word, 8> initial{0xC1059ED8U, 0x367CD507U, 0x3070DD17U, 0xF70E5939U,
                                               0xFFC00B31U, 0x68581511U, 0x64F98FA7U, 0xBEFA4FA4U};
};

template <> struct parameters<sha_variant::sha256> {
  using word = std::uint32_t;
  static constexpr std::size_t block_size = 64;
  static constexpr std::size_t digest_size = 32;
  static constexpr std::array<word, 8> initial{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                               0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
};

template <> struct parameters<sha_variant::sha384> {
  using word = std::uint64_t;
  static constexpr std::size_t block_size = 128;
  static constexpr std::size_t digest_size = 48;
  static constexpr std::array<word, 8> initial{0xCBBB9D5DC1059ED8U, 0x629A292A367CD507U, 0x9159015A3070DD17U, 0x152FECD8F70E5939U,
                                               0x67332667FFC00B31U, 0x8EB44A8768581511U, 0xDB0C2E0D64F98FA7U, 0x47B5481DBEFA4FA4U};
};

template <> struct parameters<sha_variant::sha512> {
  using word = std::uint64_t;
  static constexpr std::size_t block_size = 128;
  static constexpr std::size_t digest_size = 64;
  static constexpr std::array<word, 8> initial{0x6A09E667F3BCC908U, 0xBB67AE8584CAA73BU, 0x3C6EF372FE94F82BU, 0xA54FF53A5F1D36F1U,
                                               0x510E527FADE682D1U, 0x9B05688C2B3E6C1FU, 0x1F83D9ABFB41BD6BU, 0x5BE0CD19137E2179U};
};

template <> struct parameters<sha_variant::sha512_224> {
  using word = std::uint64_t;
  static constexpr std::size_t block_size = 128;
  static constexpr std::size_t digest_size = 28;
  static constexpr std::array<word, 8> initial{0x8C3D37C819544DA2U, 0x73E1996689DCD4D6U, 0x1DFAB7AE32FF9C82U, 0x679DD514582F9FCFU,
                                               0x0F6D2B697BD44DA8U, 0x77E36F7304C48942U, 0x3F9D85A86A1D36C8U, 0x1112E6AD91D692A1U};
};

template <> struct parameters<sha_variant::sha512_256> {
  using word = std::uint64_t;
  static constexpr std::size_t block_size = 128;
  static constexpr std::size_t digest_size = 32;
  static constexpr std::array<word, 8> initial{0x22312194FC2BF72CU, 0x9F555FA3C84C64C2U, 0x2393B86B6F53B151U, 0x963877195940EABDU,
                                               0x96283EE2A88EFFE3U, 0xBE5E1E2553863992U, 0x2B0199FC2C85B8AAU, 0x0EB72DDC81C52CA2U};
};

// Reads a big-endian integer from the first sizeof(Word) bytes at data.
template <class Word> constexpr Word load_big(std::byte const *data) noexcept;

// Folds one block into words: the compression function of Core (core()), one round per iteration or unrolled.
template <sha_variant Core> constexpr void compress_compact(word_array<Core> &words, std::byte const *block) noexcept;
template <sha_variant Core> constexpr void compress_unrolled(word_array<Core> &words, std::byte const *block) noexcept;

// compress_compact() or compress_unrolled(), as Strategy selects.
template <sha_variant Core, class Strategy> constexpr void compress(word_array<Core> &words, std::byte const *block) noexcept;

// Folds the whole blocks of data into words.
template <sha_variant Core, class Strategy> constexpr void fold_blocks(word_array<Core> &words, std::span<std::byte const> data) noexcept;

// fold_blocks(); defined once in src/sha/block_loop.cpp for each core and strategy, the loop of every preset.
template <sha_variant Core, class Strategy> void block_loop(word_array<Core> &words, std::span<std::byte const> data) noexcept;

// fold_blocks() during constant evaluation, else block_loop().
template <sha_variant Core, class Strategy> constexpr void fold(word_array<Core> &words, std::span<std::byte const> data) noexcept;

} // namespace checksum::sha_detail

///@}
///@}

namespace checksum::sha_detail {

constexpr sha_variant core(sha_variant variant) noexcept {
  switch(variant) {
  case sha_variant::sha1:
    return sha_variant::sha1;
  case sha_variant::sha224:
  case sha_variant::sha256:
    return sha_variant::sha256;
  default:
    return sha_variant::sha512;
  }
}

template <class Word> constexpr Word load_big(std::byte const *data) noexcept { return std::byteswap(detail::load<Word>(data)); }

// One SHA-1 round of phase Phase (0-3): e gains the round's sum, b is rotated. From round 16 on, the round first extends the
// schedule: in the round loop GCC does not vectorize it, which stalls on the dependency three words back.
template <unsigned Phase>
constexpr void sha1_round(std::uint32_t a, std::uint32_t &b, std::uint32_t c, std::uint32_t d, std::uint32_t &e,
                          std::array<std::uint32_t, 80> &schedule, std::size_t round) noexcept {
  if(round >= 16) {
    schedule[round] = std::rotl(schedule[round - 3] ^ schedule[round - 8] ^ schedule[round - 14] ^ schedule[round - 16], 1);
  }
  std::uint32_t mixed = 0;
  if constexpr(Phase == 0) {
    mixed = d ^ (b & (c ^ d));
  } else if constexpr(Phase == 2) {
    mixed = (b & c) | (d & (b | c));
  } else {
    mixed = b ^ c ^ d;
  }
  e += std::rotl(a, 5) + mixed + sha1_constants[Phase] + schedule[round];
  b = std::rotl(b, 30);
}

// Rounds 20 * Phase to 20 * Phase + 19 of SHA-1, five per iteration so that the variables rotate by name.
template <unsigned Phase>
constexpr void sha1_phase(std::uint32_t &a, std::uint32_t &b, std::uint32_t &c, std::uint32_t &d, std::uint32_t &e,
                          std::array<std::uint32_t, 80> &schedule) noexcept {
  for(std::size_t round = Phase * 20; round < (Phase + 1) * 20; round += 5) {
    sha1_round<Phase>(a, b, c, d, e, schedule, round);
    sha1_round<Phase>(e, a, b, c, d, schedule, round + 1);
    sha1_round<Phase>(d, e, a, b, c, schedule, round + 2);
    sha1_round<Phase>(c, d, e, a, b, schedule, round + 3);
    sha1_round<Phase>(b, c, d, e, a, schedule, round + 4);
  }
}

// One SHA-256 or SHA-512 round: d and h gain the round's sums.
template <class Value>
constexpr void sha2_round(Value a, Value b, Value c, Value &d, Value e, Value f, Value g, Value &h, Value constant_and_word) noexcept {
  constexpr bool wide = sizeof(Value) == 8;
  // The rotations of FIPS 180-4: Σ1, Σ0.
  constexpr std::array<int, 3> big_1 = wide ? std::array{14, 18, 41} : std::array{6, 11, 25};
  constexpr std::array<int, 3> big_0 = wide ? std::array{28, 34, 39} : std::array{2, 13, 22};
  Value const first = h + (std::rotr(e, big_1[0]) ^ std::rotr(e, big_1[1]) ^ std::rotr(e, big_1[2])) + (g ^ (e & (f ^ g))) + constant_and_word;
  d += first;
  h = first + (std::rotr(a, big_0[0]) ^ std::rotr(a, big_0[1]) ^ std::rotr(a, big_0[2])) + ((a & b) | (c & (a | b)));
}

template <sha_variant Core> constexpr void compress_unrolled(word_array<Core> &words, std::byte const *block) noexcept {
  using value = word<Core>;
  constexpr std::size_t size = sizeof(value);
  if constexpr(Core == sha_variant::sha1) {
    std::array<value, 80> schedule{};
    for(std::size_t index = 0; index < 16; ++index) {
      schedule[index] = load_big<value>(block + (index * size));
    }
    auto [a, b, c, d, e] = words;
    sha1_phase<0>(a, b, c, d, e, schedule);
    sha1_phase<1>(a, b, c, d, e, schedule);
    sha1_phase<2>(a, b, c, d, e, schedule);
    sha1_phase<3>(a, b, c, d, e, schedule);
    words = {words[0] + a, words[1] + b, words[2] + c, words[3] + d, words[4] + e};
  } else {
    // σ0 and σ1 of FIPS 180-4, the shift last.
    constexpr bool wide = Core == sha_variant::sha512;
    constexpr std::array<int, 3> small_0 = wide ? std::array{1, 8, 7} : std::array{7, 18, 3};
    constexpr std::array<int, 3> small_1 = wide ? std::array{19, 61, 6} : std::array{17, 19, 10};
    constexpr std::size_t rounds = wide ? 80 : 64;
    std::array<value, rounds> schedule{};
    for(std::size_t index = 0; index < 16; ++index) {
      schedule[index] = load_big<value>(block + (index * size));
    }
    for(std::size_t index = 16; index < rounds; ++index) {
      value const early = schedule[index - 15];
      value const late = schedule[index - 2];
      schedule[index] = schedule[index - 16] + (std::rotr(early, small_0[0]) ^ std::rotr(early, small_0[1]) ^ (early >> small_0[2])) +
                        schedule[index - 7] + (std::rotr(late, small_1[0]) ^ std::rotr(late, small_1[1]) ^ (late >> small_1[2]));
    }
    for(std::size_t index = 0; index < rounds; ++index) {
      if constexpr(wide) {
        schedule[index] += sha512_constants[index];
      } else {
        schedule[index] += sha256_constants[index];
      }
    }
    auto [a, b, c, d, e, f, g, h] = words;
    // Eight rounds per iteration, so that the variables rotate by name.
    for(std::size_t round = 0; round < rounds; round += 8) {
      sha2_round(a, b, c, d, e, f, g, h, schedule[round]);
      sha2_round(h, a, b, c, d, e, f, g, schedule[round + 1]);
      sha2_round(g, h, a, b, c, d, e, f, schedule[round + 2]);
      sha2_round(f, g, h, a, b, c, d, e, schedule[round + 3]);
      sha2_round(e, f, g, h, a, b, c, d, schedule[round + 4]);
      sha2_round(d, e, f, g, h, a, b, c, schedule[round + 5]);
      sha2_round(c, d, e, f, g, h, a, b, schedule[round + 6]);
      sha2_round(b, c, d, e, f, g, h, a, schedule[round + 7]);
    }
    words = {words[0] + a, words[1] + b, words[2] + c, words[3] + d, words[4] + e, words[5] + f, words[6] + g, words[7] + h};
  }
}

template <sha_variant Core> constexpr void compress_compact(word_array<Core> &words, std::byte const *block) noexcept {
  using value = word<Core>;
  constexpr std::size_t size = sizeof(value);
  std::array<value, 16> schedule{};
  for(std::size_t index = 0; index < schedule.size(); ++index) {
    schedule[index] = load_big<value>(block + (index * size));
  }
  if constexpr(Core == sha_variant::sha1) {
    auto [a, b, c, d, e] = words;
    for(std::size_t round = 0; round < 80; ++round) {
      if(round >= 16) {
        schedule[round % 16] =
            std::rotl(schedule[(round - 3) % 16] ^ schedule[(round - 8) % 16] ^ schedule[(round - 14) % 16] ^ schedule[round % 16], 1);
      }
      value const mixed = round < 20 ? (b & c) | (~b & d) : round < 40 || round >= 60 ? b ^ c ^ d : (b & c) | (b & d) | (c & d);
      value const next = std::rotl(a, 5) + mixed + e + sha1_constants[round / 20] + schedule[round % 16];
      e = d;
      d = c;
      c = std::rotl(b, 30);
      b = a;
      a = next;
    }
    words = {words[0] + a, words[1] + b, words[2] + c, words[3] + d, words[4] + e};
  } else {
    // The rotations of FIPS 180-4 for SHA-256 and SHA-512: Σ0, Σ1, then σ0 and σ1 with their shifts last.
    constexpr bool wide = Core == sha_variant::sha512;
    constexpr std::array<int, 3> big_0 = wide ? std::array{28, 34, 39} : std::array{2, 13, 22};
    constexpr std::array<int, 3> big_1 = wide ? std::array{14, 18, 41} : std::array{6, 11, 25};
    constexpr std::array<int, 3> small_0 = wide ? std::array{1, 8, 7} : std::array{7, 18, 3};
    constexpr std::array<int, 3> small_1 = wide ? std::array{19, 61, 6} : std::array{17, 19, 10};
    constexpr std::size_t rounds = wide ? 80 : 64;
    auto [a, b, c, d, e, f, g, h] = words;
    for(std::size_t round = 0; round < rounds; ++round) {
      if(round >= 16) {
        value const early = schedule[(round - 15) % 16];
        value const late = schedule[(round - 2) % 16];
        schedule[round % 16] += (std::rotr(early, small_0[0]) ^ std::rotr(early, small_0[1]) ^ (early >> small_0[2])) + schedule[(round - 7) % 16] +
                                (std::rotr(late, small_1[0]) ^ std::rotr(late, small_1[1]) ^ (late >> small_1[2]));
      }
      value constant = 0;
      if constexpr(wide) {
        constant = sha512_constants[round];
      } else {
        constant = sha256_constants[round];
      }
      value const first =
          h + (std::rotr(e, big_1[0]) ^ std::rotr(e, big_1[1]) ^ std::rotr(e, big_1[2])) + ((e & f) ^ (~e & g)) + constant + schedule[round % 16];
      value const second = (std::rotr(a, big_0[0]) ^ std::rotr(a, big_0[1]) ^ std::rotr(a, big_0[2])) + ((a & b) ^ (a & c) ^ (b & c));
      h = g;
      g = f;
      f = e;
      e = d + first;
      d = c;
      c = b;
      b = a;
      a = first + second;
    }
    words = {words[0] + a, words[1] + b, words[2] + c, words[3] + d, words[4] + e, words[5] + f, words[6] + g, words[7] + h};
  }
}

template <sha_variant Core, class Strategy> constexpr void compress(word_array<Core> &words, std::byte const *block) noexcept {
  if constexpr(std::same_as<Strategy, sha_unrolled>) {
    compress_unrolled<Core>(words, block);
  } else {
    compress_compact<Core>(words, block);
  }
}

template <sha_variant Core, class Strategy> constexpr void fold_blocks(word_array<Core> &words, std::span<std::byte const> data) noexcept {
  constexpr std::size_t block_size = parameters<Core>::block_size;
  for(std::size_t offset = 0; offset + block_size <= data.size(); offset += block_size) {
    compress<Core, Strategy>(words, data.data() + offset);
  }
}

template <sha_variant Core, class Strategy> constexpr void fold(word_array<Core> &words, std::span<std::byte const> data) noexcept {
  if consteval {
    fold_blocks<Core, Strategy>(words, data);
  } else {
    block_loop<Core, Strategy>(words, data);
  }
}

} // namespace checksum::sha_detail

namespace checksum {

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
constexpr isa sha_engine<Variant, Strategy, Isa>::preset() noexcept {
  return Isa;
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
constexpr sha_state<Variant> sha_engine<Variant, Strategy, Isa>::initial() const noexcept {
  return {};
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
constexpr sha_state<Variant> sha_engine<Variant, Strategy, Isa>::update(sha_state<Variant> state, std::span<std::byte const> data) const noexcept {
  constexpr sha_variant core = sha_detail::core(Variant);
  constexpr std::size_t block_size = sha_detail::parameters<Variant>::block_size;
  auto const buffered = static_cast<std::size_t>(state.length % block_size);
  state.length += data.size();
  if(buffered != 0) {
    std::size_t const fill = std::min(block_size - buffered, data.size());
    std::ranges::copy(data.first(fill), state.buffer.begin() + static_cast<std::ptrdiff_t>(buffered));
    data = data.subspan(fill);
    if(buffered + fill < block_size) {
      return state;
    }
    sha_detail::compress<core, Strategy>(state.words, state.buffer.data());
  }
  std::size_t const whole = data.size() - (data.size() % block_size);
  if(whole != 0) {
    sha_detail::fold<core, Strategy>(state.words, data.first(whole));
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
  return state;
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
template <byte_range Range>
constexpr sha_state<Variant> sha_engine<Variant, Strategy, Isa>::update(sha_state<Variant> state, Range &&data) const noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = update(state, chunk); });
  return state;
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
constexpr sha_engine<Variant, Strategy, Isa>::value_type
sha_engine<Variant, Strategy, Isa>::finalize(sha_state<Variant> const &state) const noexcept {
  constexpr sha_variant core = sha_detail::core(Variant);
  using word = sha_detail::word<Variant>;
  constexpr std::size_t block_size = sha_detail::parameters<Variant>::block_size;
  // The bit count, big-endian in the last 8 bytes of the block; SHA-384 and SHA-512 have 16 bytes, whose first 8 stay zero
  // below 2^61 bytes.
  constexpr std::size_t length_size = 2 * sizeof(word);
  auto const buffered = static_cast<std::size_t>(state.length % block_size);
  std::array<std::byte, block_size> block = state.buffer;
  auto words = state.words;
  block[buffered] = std::byte{0x80};
  std::ranges::fill(std::span(block).subspan(buffered + 1), std::byte{0});
  if(buffered + 1 > block_size - length_size) {
    sha_detail::compress<core, Strategy>(words, block.data());
    block.fill(std::byte{0});
  }
  std::uint64_t const bits = state.length * 8;
  for(std::size_t index = 0; index < 8; ++index) {
    block[block_size - 1 - index] = static_cast<std::byte>(bits >> (8 * index));
  }
  sha_detail::compress<core, Strategy>(words, block.data());
  value_type digest{};
  for(std::size_t index = 0; index < digest.size(); ++index) {
    digest[index] = static_cast<std::uint8_t>(words[index / sizeof(word)] >> (8 * (sizeof(word) - 1 - (index % sizeof(word)))));
  }
  return digest;
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
constexpr sha_engine<Variant, Strategy, Isa>::value_type sha_engine<Variant, Strategy, Isa>::compute(std::span<std::byte const> data) const noexcept {
  return finalize(update(initial(), data));
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa>
  requires(isa_available(Isa))
template <byte_range Range>
constexpr sha_engine<Variant, Strategy, Isa>::value_type sha_engine<Variant, Strategy, Isa>::compute(Range &&data) const noexcept {
  return finalize(update(initial(), std::forward<Range>(data)));
}

template <sha_variant Variant, sha_strategy Strategy, isa Isa, byte_range Range>
constexpr sha_engine<Variant, Strategy, Isa>::value_type sha_compute(Range &&data) noexcept {
  return sha_engine<Variant, Strategy, Isa>{}.compute(std::forward<Range>(data));
}

template <sha_variant Variant, sha_strategy Strategy>
constexpr sha_dispatch_engine<Variant, Strategy>::sha_dispatch_engine(isa_set cpu) noexcept
    : detail::dispatch_engine<sha_engine<Variant, Strategy>, sha_detail::own_loops>(cpu) {}

} // namespace checksum

#endif // CHECKSUM_SHA_HPP
