#ifndef CHECKSUM_XXH3_HPP
#define CHECKSUM_XXH3_HPP

#include <checksum/byte_range.hpp>
#include <checksum/hash128.hpp>
#include <checksum/xxhash.hpp>

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
 *   @defgroup checksum_xxh3 XXH3
 *   The non-cryptographic hashes XXH3-64 and XXH3-128 with a seed and the default secret, at compile time or at run time.
 *   @{
 */

namespace checksum {

/// Running XXH3-64 (Width 64) or XXH3-128 (Width 128) hash, 336 bytes. Every value is valid; the default is the empty message with seed 0, and
/// xxh3_state<Width>{.seed = seed} starts one with another seed.
template <unsigned Width>
  requires(Width == 64 || Width == 128)
struct xxh3_state {
  /// The hash type: std::uint64_t or hash128.
  using value_type = std::conditional_t<Width == 64, std::uint64_t, hash128>;

  /// Lane accumulators of the folded input: all but the last 1 to 256 bytes, rounded down to a multiple of 256.
  std::array<std::uint64_t, 8> accumulators{0xC2B2AE3DU,         0x9E3779B185EBCA87U, 0xC2B2AE3D27D4EB4FU, 0x165667B19E3779F9U,
                                            0x85EBCA77C2B2AE63U, 0x85EBCA77U,         0x27D4EB2F165667C5U, 0x9E3779B1U};
  std::uint64_t length = 0; ///< Bytes passed so far.
  std::uint64_t seed = 0;   ///< Seed of the hash.
  std::array<std::byte, 256>
      buffer{}; ///< The input after the folded part, from the start. While it holds fewer than 64 bytes, its end keeps the stripe before them.
};

/// Folds data into state; data may be split anywhere. With a seed other than 0, each call that folds the buffer builds the 192-byte
/// secret again, as xxh3_finalize() does: on x86-64 about 25 % slower than seed 0 in 256-byte pieces, 6 % in 4 KiB pieces.
template <unsigned Width> [[nodiscard]] constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, std::span<std::byte const> data) noexcept;

/// Folds a byte range into state. Contiguous ranges are passed on as one span, others in 64-byte chunks.
template <unsigned Width, byte_range Range> [[nodiscard]] constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, Range &&data) noexcept;

/// The hash of the message folded into state.
template <unsigned Width> [[nodiscard]] constexpr xxh3_state<Width>::value_type xxh3_finalize(xxh3_state<Width> const &state) noexcept;

/// The hash of data: xxh3_finalize(xxh3_update(xxh3_state<Width>{.seed = seed}, data)).
template <unsigned Width>
[[nodiscard]] constexpr xxh3_state<Width>::value_type xxh3_compute(std::span<std::byte const> data, std::uint64_t seed = 0) noexcept;

/// The hash of a byte range with a seed. A std::span<std::byte const> takes the overload above.
template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
[[nodiscard]] constexpr xxh3_state<Width>::value_type xxh3_compute(Range &&data, std::uint64_t seed = 0) noexcept;

/// Running XXH3-64 hash.
using xxh3_64_state = xxh3_state<64>;

/// Running XXH3-128 hash.
using xxh3_128_state = xxh3_state<128>;

/// The XXH3-64 hash of data: xxh3_compute<64>(data, seed).
template <byte_range Range> [[nodiscard]] constexpr std::uint64_t xxh3_64_compute(Range &&data, std::uint64_t seed = 0) noexcept;

/// The XXH3-128 hash of data: xxh3_compute<128>(data, seed).
template <byte_range Range> [[nodiscard]] constexpr hash128 xxh3_128_compute(Range &&data, std::uint64_t seed = 0) noexcept;

} // namespace checksum

/// Implementation details; not part of the public API.
namespace checksum::xxh3_detail {

using xxhash_detail::load;

template <unsigned Width> using value = xxh3_state<Width>::value_type;

using accumulator_array = std::array<std::uint64_t, 8>;

inline constexpr std::size_t stripe_size = 64;
inline constexpr std::size_t secret_size = 192;
inline constexpr std::size_t buffer_size = std::tuple_size_v<decltype(xxh3_state<64>::buffer)>;
inline constexpr std::size_t small_max = 16;
inline constexpr std::size_t medium_max = 128;
inline constexpr std::size_t midsize_max = 240;

// Sizes of and offsets into the secret, and the bytes of secret each stripe advances, named as in the reference implementation.
inline constexpr std::size_t secret_size_min = 136;
inline constexpr std::size_t midsize_start_offset = 3;
inline constexpr std::size_t midsize_last_offset = 17;
inline constexpr std::size_t secret_consume_rate = 8;
inline constexpr std::size_t secret_last_accumulate_start = 7;
inline constexpr std::size_t secret_merge_start = 11;

// Stripes of a block, after which the accumulators are scrambled.
inline constexpr std::size_t stripes_per_block = (secret_size - stripe_size) / secret_consume_rate;

using secret_array = std::array<std::byte, secret_size>;

using primes_32 = xxhash_detail::algorithm_constants<32>;
using primes_64 = xxhash_detail::algorithm_constants<64>;
inline constexpr std::uint64_t prime_mx1 = 0x165667919E3779F9U;
inline constexpr std::uint64_t prime_mx2 = 0x9FB21C651E98DF25U;

// Shifts and rotations of the mixing steps.
inline constexpr std::uint64_t low_half_mask = 0xFFFFFFFFU;
inline constexpr unsigned avalanche_shift = 37;
inline constexpr unsigned scramble_shift = 47;
inline constexpr unsigned length_shift = 54;
inline constexpr std::array<int, 2> mix_rotations{49, 24};
inline constexpr std::array<unsigned, 2> mix_shifts{35, 28};

inline constexpr accumulator_array initial_accumulators = xxh3_state<64>{}.accumulators;

// The default secret, read as little-endian 64-bit words.
inline constexpr std::array<std::uint64_t, secret_size / 8> default_secret_words{
    0xBE4BA423396CFEB8U, 0x1CAD21F72C81017CU, 0xDB979083E96DD4DEU, 0x1F67B3B7A4A44072U, 0x78E5C0CC4EE679CBU, 0x2172FFCC7DD05A82U,
    0x8E2443F7744608B8U, 0x4C263A81E69035E0U, 0xCB00C391BB52283CU, 0xA32E531B8B65D088U, 0x4EF90DA297486471U, 0xD8ACDEA946EF1938U,
    0x3F349CE33F76FAA8U, 0x1D4F0BC7C7BBDCF9U, 0x3159B4CD4BE0518AU, 0x647378D9C97E9FC8U, 0xC3EBD33483ACC5EAU, 0xEB6313FAFFA081C5U,
    0x49DAF0B751DD0D17U, 0x9E68D429265516D3U, 0xFCA1477D58BE162BU, 0xCE31D07AD1B8F88FU, 0x280416958F3ACB45U, 0x7E404BBBCAFBD7AFU};

// The full product of two 64-bit values, from four 32-bit products.
constexpr hash128 multiply_portable(std::uint64_t left, std::uint64_t right) noexcept;

// multiply_portable(), or a native 128-bit multiplication at run time where the compiler has one.
constexpr hash128 multiply(std::uint64_t left, std::uint64_t right) noexcept;

// The exclusive or of the two Integer words at secret.
template <class Integer> constexpr Integer secret_xor(std::byte const *secret) noexcept;

// The halves of the full product, combined by exclusive or; run_time_multiply_fold() at run time without a 128-bit type.
constexpr std::uint64_t multiply_fold(std::uint64_t left, std::uint64_t right) noexcept;

// multiply_fold(), defined in src/xxh3/stripe_loop.cpp. Inlined, its four multiplications make the 17-128 byte path too long for the
// 1 KiB instruction cache of an STM32L4: 128 bytes at 14 instead of 19 MB/s.
std::uint64_t run_time_multiply_fold(std::uint64_t left, std::uint64_t right) noexcept;

constexpr std::uint64_t avalanche(std::uint64_t hash) noexcept;

// The default secret with the seed added to its even words and subtracted from its odd ones.
constexpr secret_array make_secret(std::uint64_t seed) noexcept;

// function(secret) with the secret of seed: the default one in read-only data for seed 0, else one made on the stack.
template <class Function> constexpr auto with_secret(std::uint64_t seed, Function function) noexcept;

// Mixes 16 bytes of data with 16 bytes of the secret and the seed.
constexpr std::uint64_t mix_step(std::byte const *data, std::byte const *secret, std::uint64_t seed) noexcept;

// Mixes two 16-byte chunks into both halves of a 128-bit accumulator.
constexpr void mix_two_chunks(hash128 &accumulator, std::byte const *first, std::byte const *second, std::byte const *secret,
                              std::uint64_t seed) noexcept;

// The hash of 0 to 16 bytes.
template <unsigned Width> constexpr value<Width> hash_small(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// The 128-bit hash of 17 to 240 bytes from its accumulator.
constexpr hash128 finish_medium(hash128 const &accumulator, std::uint64_t length, std::uint64_t seed) noexcept;

// The hash of 17 to 128 bytes.
template <unsigned Width> constexpr value<Width> hash_medium(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// The hash of 129 to 240 bytes.
template <unsigned Width> constexpr value<Width> hash_midsize(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// Mixes lane Lane of one stripe into the accumulators.
template <std::size_t Lane>
constexpr void accumulate_lane(accumulator_array &accumulators, std::byte const *stripe, std::byte const *secret) noexcept;

// The portable stripe kernel. A kernel keeps the accumulators in its lanes type and defines the same four functions.
struct portable_kernel {
  using lanes = accumulator_array;
  static constexpr lanes load(accumulator_array const &accumulators) noexcept;
  static constexpr void store(accumulator_array &accumulators, lanes const &values) noexcept;
  // Mixes one stripe into the accumulators. Spelled out per lane: as a loop, GCC keeps the accumulators in memory; GCC for
  // Cortex-M4 outlines the unforced lanes and calls them per stripe.
  static constexpr void accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept;
  // Scrambles the accumulators with the last 64 bytes of the secret, at secret.
  static constexpr void scramble(lanes &values, std::byte const *secret) noexcept;
};

// Folds the whole stripes of data into the accumulators with Kernel, the first one block_stripe stripes into a block, scrambling them
// after the last stripe of each block. Then mixes in the last stripe of the message at last_stripe, unless it is null.
template <class Kernel>
constexpr void fold_stripes(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                            std::byte const *last_stripe) noexcept;

// fold_stripes(), defined in src/xxh3/stripe_loop.cpp.
void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                 std::byte const *last_stripe) noexcept;

// fold_stripes() during constant evaluation, else stripe_loop().
constexpr void fold(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                    std::byte const *last_stripe) noexcept;

// Merges the accumulators into 64 bits with 64 bytes of the secret.
constexpr std::uint64_t merge(accumulator_array const &accumulators, std::byte const *secret, std::uint64_t initial) noexcept;

// The hash of a message of length bytes over 240, from the accumulators of all its stripes.
template <unsigned Width>
constexpr value<Width> finish_large(accumulator_array const &accumulators, std::uint64_t length, secret_array const &secret) noexcept;

// The hash of more than 240 bytes.
template <unsigned Width> constexpr value<Width> hash_large(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// Bytes of a message of length bytes that a state has folded into its accumulators.
constexpr std::uint64_t folded_length(std::uint64_t length) noexcept;

// Position in its block of the stripe at offset folded.
constexpr std::size_t stripe_index(std::uint64_t folded) noexcept;

// xxh3_update() in place: copying the state costs more than folding a short piece.
template <unsigned Width> constexpr void update(xxh3_state<Width> &state, std::span<std::byte const> data) noexcept;

} // namespace checksum::xxh3_detail

///@}
///@}

namespace checksum::xxh3_detail {

constexpr hash128 multiply_portable(std::uint64_t left, std::uint64_t right) noexcept {
  std::uint64_t const low_low = (left & low_half_mask) * (right & low_half_mask);
  std::uint64_t const high_low = (left >> 32U) * (right & low_half_mask);
  std::uint64_t const low_high = (left & low_half_mask) * (right >> 32U);
  std::uint64_t const high_high = (left >> 32U) * (right >> 32U);
  std::uint64_t const cross = (low_low >> 32U) + (high_low & low_half_mask) + low_high;
  return {.low = (cross << 32U) | (low_low & low_half_mask), .high = (high_low >> 32U) + (cross >> 32U) + high_high};
}

constexpr hash128 multiply(std::uint64_t left, std::uint64_t right) noexcept {
#ifdef __SIZEOF_INT128__
  if !consteval {
    __extension__ typedef unsigned __int128 wide_type; // NOLINT(modernize-use-using): __extension__ silences -Wpedantic only on a typedef.
    wide_type const product = static_cast<wide_type>(left) * right;
    return {.low = static_cast<std::uint64_t>(product), .high = static_cast<std::uint64_t>(product >> 64U)};
  }
#endif
  return multiply_portable(left, right);
}

template <class Integer> constexpr Integer secret_xor(std::byte const *secret) noexcept {
  return load<Integer>(secret) ^ load<Integer>(secret + sizeof(Integer));
}

constexpr std::uint64_t multiply_fold(std::uint64_t left, std::uint64_t right) noexcept {
#ifndef __SIZEOF_INT128__
  if !consteval {
    return run_time_multiply_fold(left, right);
  }
#endif
  hash128 const product = multiply(left, right);
  return product.low ^ product.high;
}

constexpr std::uint64_t avalanche(std::uint64_t hash) noexcept {
  hash ^= hash >> avalanche_shift;
  hash *= prime_mx1;
  return hash ^ (hash >> 32U);
}

constexpr secret_array make_secret(std::uint64_t seed) noexcept {
  std::array<std::uint64_t, secret_size / 8> words = default_secret_words;
  for(std::size_t index = 0; index < words.size(); ++index) {
    words[index] = index % 2 == 0 ? words[index] + seed : words[index] - seed;
    if constexpr(std::endian::native == std::endian::big) {
      words[index] = std::byteswap(words[index]);
    }
  }
  return std::bit_cast<secret_array>(words);
}

inline constexpr secret_array default_secret = make_secret(0);

template <class Function> constexpr auto with_secret(std::uint64_t seed, Function function) noexcept {
  if(seed == 0) {
    return function(default_secret);
  }
  secret_array const secret = make_secret(seed);
  return function(secret);
}

constexpr std::uint64_t mix_step(std::byte const *data, std::byte const *secret, std::uint64_t seed) noexcept {
  return multiply_fold(load<std::uint64_t>(data) ^ (load<std::uint64_t>(secret) + seed),
                       load<std::uint64_t>(data + 8) ^ (load<std::uint64_t>(secret + 8) - seed));
}

constexpr void mix_two_chunks(hash128 &accumulator, std::byte const *first, std::byte const *second, std::byte const *secret,
                              std::uint64_t seed) noexcept {
  accumulator.low += mix_step(first, secret, seed);
  accumulator.low ^= load<std::uint64_t>(second) + load<std::uint64_t>(second + 8);
  accumulator.high += mix_step(second, secret + 16, seed);
  accumulator.high ^= load<std::uint64_t>(first) + load<std::uint64_t>(first + 8);
}

template <unsigned Width> constexpr value<Width> hash_small(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  std::byte const *input = data.data();
  std::byte const *secret = default_secret.data();
  std::uint64_t const length = data.size();
  if(length > 8) {
    auto const first = load<std::uint64_t>(input);
    auto const last = load<std::uint64_t>(input + length - 8);
    if constexpr(Width == 64) {
      constexpr std::size_t offset = 24;
      std::uint64_t const low = (secret_xor<std::uint64_t>(secret + offset) + seed) ^ first;
      std::uint64_t const high = (secret_xor<std::uint64_t>(secret + offset + 16) - seed) ^ last;
      return avalanche(length + std::byteswap(low) + high + multiply_fold(low, high));
    } else {
      constexpr std::size_t offset = 32;
      std::uint64_t const mixed_first = (secret_xor<std::uint64_t>(secret + offset) - seed) ^ first ^ last;
      std::uint64_t const mixed_last = (secret_xor<std::uint64_t>(secret + offset + 16) + seed) ^ last;
      hash128 product = multiply(mixed_first, primes_64::prime_1);
      product.low += (length - 1) << length_shift;
      product.high += mixed_last + ((mixed_last & low_half_mask) * (primes_32::prime_2 - 1));
      product.low ^= std::byteswap(product.high);
      hash128 const result = multiply(product.low, primes_64::prime_2);
      return {.low = avalanche(result.low), .high = avalanche(result.high + (product.high * primes_64::prime_2))};
    }
  }
  if(length >= 4) {
    std::uint64_t const first = load<std::uint32_t>(input);
    std::uint64_t const last = load<std::uint32_t>(input + length - 4);
    std::uint64_t const modified_seed = seed ^ (std::uint64_t{std::byteswap(static_cast<std::uint32_t>(seed))} << 32U);
    if constexpr(Width == 64) {
      std::uint64_t mixed = (secret_xor<std::uint64_t>(secret + 8) - modified_seed) ^ (last | (first << 32U));
      mixed ^= std::rotl(mixed, mix_rotations[0]) ^ std::rotl(mixed, mix_rotations[1]);
      mixed *= prime_mx2;
      mixed ^= (mixed >> mix_shifts[0]) + length;
      mixed *= prime_mx2;
      return mixed ^ (mixed >> mix_shifts[1]);
    } else {
      std::uint64_t const mixed = (secret_xor<std::uint64_t>(secret + 16) + modified_seed) ^ (first | (last << 32U));
      hash128 const product = multiply(mixed, primes_64::prime_1 + (length << 2U));
      std::uint64_t const high = product.high + (product.low << 1U);
      std::uint64_t low = product.low ^ (high >> 3U);
      low ^= low >> mix_shifts[0];
      low *= prime_mx2;
      return {.low = low ^ (low >> mix_shifts[1]), .high = avalanche(high)};
    }
  }
  if(length > 0) {
    std::uint32_t const combined = std::to_integer<std::uint32_t>(input[length - 1]) | static_cast<std::uint32_t>(length << 8U) |
                                   (std::to_integer<std::uint32_t>(input[0]) << 16U) | (std::to_integer<std::uint32_t>(input[length / 2]) << 24U);
    std::uint64_t const low = (std::uint64_t{secret_xor<std::uint32_t>(secret)} + seed) ^ combined;
    if constexpr(Width == 64) {
      return xxhash_detail::avalanche<64>(low);
    } else {
      std::uint64_t const high = (std::uint64_t{secret_xor<std::uint32_t>(secret + 8)} - seed) ^ std::rotl(std::byteswap(combined), 13);
      return {.low = xxhash_detail::avalanche<64>(low), .high = xxhash_detail::avalanche<64>(high)};
    }
  }
  if constexpr(Width == 64) {
    constexpr std::size_t offset = 56;
    return xxhash_detail::avalanche<64>(seed ^ secret_xor<std::uint64_t>(secret + offset));
  } else {
    return {.low = xxhash_detail::avalanche<64>(seed ^ secret_xor<std::uint64_t>(secret + 64)),
            .high = xxhash_detail::avalanche<64>(seed ^ secret_xor<std::uint64_t>(secret + 64 + 16))};
  }
}

constexpr hash128 finish_medium(hash128 const &accumulator, std::uint64_t length, std::uint64_t seed) noexcept {
  std::uint64_t const high =
      (accumulator.low * primes_64::prime_1) + (accumulator.high * primes_64::prime_4) + ((length - seed) * primes_64::prime_2);
  return {.low = avalanche(accumulator.low + accumulator.high), .high = 0 - avalanche(high)};
}

template <unsigned Width> constexpr value<Width> hash_medium(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  std::byte const *input = data.data();
  std::byte const *secret = default_secret.data();
  std::size_t const length = data.size();
  // For XXH3-128, the low half; the high one starts at 0.
  value<Width> accumulator{length * primes_64::prime_1};
  // Round index mixes the chunks index * 16 bytes from either end.
  auto const round = [&](std::size_t index) {
    std::byte const *first = input + (16 * index);
    std::byte const *last = input + length - (16 * index) - 16;
    if constexpr(Width == 64) {
      accumulator += mix_step(first, secret + (32 * index), seed);
      accumulator += mix_step(last, secret + (32 * index) + 16, seed);
    } else {
      mix_two_chunks(accumulator, first, last, secret + (32 * index), seed);
    }
  };
  // Innermost chunks first, without a loop.
  if(length > 32) {
    if(length > 64) {
      if(length > 96) {
        round(3);
      }
      round(2);
    }
    round(1);
  }
  round(0);
  if constexpr(Width == 64) {
    return avalanche(accumulator);
  } else {
    return finish_medium(accumulator, length, seed);
  }
}

template <unsigned Width> constexpr value<Width> hash_midsize(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  std::byte const *input = data.data();
  std::byte const *secret = default_secret.data();
  std::size_t const length = data.size();
  if constexpr(Width == 64) {
    std::uint64_t accumulator = length * primes_64::prime_1;
    for(std::size_t chunk = 0; chunk < 8; ++chunk) {
      accumulator += mix_step(input + (16 * chunk), secret + (16 * chunk), seed);
    }
    accumulator = avalanche(accumulator);
    for(std::size_t chunk = 8; chunk < length / 16; ++chunk) {
      accumulator += mix_step(input + (16 * chunk), secret + (16 * (chunk - 8)) + midsize_start_offset, seed);
    }
    accumulator += mix_step(input + length - 16, secret + secret_size_min - midsize_last_offset, seed);
    return avalanche(accumulator);
  } else {
    hash128 accumulator{.low = length * primes_64::prime_1, .high = 0};
    for(std::size_t chunk = 0; chunk < 4; ++chunk) {
      mix_two_chunks(accumulator, input + (32 * chunk), input + (32 * chunk) + 16, secret + (32 * chunk), seed);
    }
    accumulator = {.low = avalanche(accumulator.low), .high = avalanche(accumulator.high)};
    for(std::size_t chunk = 4; chunk < length / 32; ++chunk) {
      mix_two_chunks(accumulator, input + (32 * chunk), input + (32 * chunk) + 16, secret + (32 * (chunk - 4)) + midsize_start_offset, seed);
    }
    mix_two_chunks(accumulator, input + length - 16, input + length - 32, secret + secret_size_min - midsize_last_offset - 16, 0 - seed);
    return finish_medium(accumulator, length, seed);
  }
}

template <std::size_t Lane>
constexpr void accumulate_lane(accumulator_array &accumulators, std::byte const *stripe, std::byte const *secret) noexcept {
  auto const word = load<std::uint64_t>(stripe + (8 * Lane));
  std::uint64_t const key = word ^ load<std::uint64_t>(secret + (8 * Lane));
  std::get<Lane ^ 1U>(accumulators) += word;
  std::get<Lane>(accumulators) += (key & low_half_mask) * (key >> 32U);
}

constexpr portable_kernel::lanes portable_kernel::load(accumulator_array const &accumulators) noexcept { return accumulators; }

constexpr void portable_kernel::store(accumulator_array &accumulators, lanes const &values) noexcept { accumulators = values; }

constexpr void portable_kernel::accumulate(lanes &values, std::byte const *stripe, std::byte const *secret) noexcept {
  [&]<std::size_t... Lane> [[gnu::always_inline]] (std::index_sequence<Lane...>) {
    (accumulate_lane<Lane>(values, stripe, secret), ...);
  }(std::make_index_sequence<std::tuple_size_v<lanes>>{});
}

constexpr void portable_kernel::scramble(lanes &values, std::byte const *secret) noexcept {
  for(std::size_t lane = 0; lane < values.size(); ++lane) {
    std::uint64_t const accumulator = values[lane] ^ (values[lane] >> scramble_shift);
    values[lane] = (accumulator ^ xxh3_detail::load<std::uint64_t>(secret + (8 * lane))) * primes_32::prime_1;
  }
}

template <class Kernel>
constexpr void fold_stripes(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                            std::byte const *last_stripe) noexcept {
  // A local copy stays in registers: stores through the reference could alias data.
  typename Kernel::lanes lanes = Kernel::load(accumulators);
  std::byte const *position = data.data();
  std::byte const *key = secret.data() + (secret_consume_rate * block_stripe);
  std::byte const *const block_end = secret.data() + (secret_consume_rate * stripes_per_block);
  for(std::size_t count = data.size() / stripe_size; count != 0; --count, position += stripe_size) {
    Kernel::accumulate(lanes, position, key);
    key += secret_consume_rate;
    if(key == block_end) {
      Kernel::scramble(lanes, secret.data() + secret_size - stripe_size);
      key = secret.data();
    }
  }
  if(last_stripe != nullptr) {
    Kernel::accumulate(lanes, last_stripe, secret.data() + secret_size - stripe_size - secret_last_accumulate_start);
  }
  Kernel::store(accumulators, lanes);
}

constexpr void fold(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                    std::byte const *last_stripe) noexcept {
  if consteval {
    fold_stripes<portable_kernel>(accumulators, data, block_stripe, secret, last_stripe);
  } else {
    stripe_loop(accumulators, data, block_stripe, secret, last_stripe);
  }
}

constexpr std::uint64_t merge(accumulator_array const &accumulators, std::byte const *secret, std::uint64_t initial) noexcept {
  std::uint64_t result = initial;
  for(std::size_t pair = 0; pair < accumulators.size(); pair += 2) {
    result += multiply_fold(accumulators[pair] ^ load<std::uint64_t>(secret + (8 * pair)),
                            accumulators[pair + 1] ^ load<std::uint64_t>(secret + (8 * pair) + 8));
  }
  return avalanche(result);
}

template <unsigned Width>
constexpr value<Width> finish_large(accumulator_array const &accumulators, std::uint64_t length, secret_array const &secret) noexcept {
  std::uint64_t const low = merge(accumulators, secret.data() + secret_merge_start, length * primes_64::prime_1);
  if constexpr(Width == 64) {
    return low;
  } else {
    return {.low = low, .high = merge(accumulators, secret.data() + secret_size - stripe_size - secret_merge_start, ~(length * primes_64::prime_2))};
  }
}

template <unsigned Width> constexpr value<Width> hash_large(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  return with_secret(seed, [data](secret_array const &secret) {
    accumulator_array accumulators = initial_accumulators;
    fold(accumulators, data.first(((data.size() - 1) / stripe_size) * stripe_size), 0, secret, data.data() + data.size() - stripe_size);
    return finish_large<Width>(accumulators, data.size(), secret);
  });
}

constexpr std::uint64_t folded_length(std::uint64_t length) noexcept { return length == 0 ? 0 : ((length - 1) / buffer_size) * buffer_size; }

constexpr std::size_t stripe_index(std::uint64_t folded) noexcept { return static_cast<std::size_t>((folded / stripe_size) % stripes_per_block); }

template <unsigned Width> constexpr void update(xxh3_state<Width> &state, std::span<std::byte const> data) noexcept {
  std::uint64_t const folded = folded_length(state.length);
  auto const buffered = static_cast<std::size_t>(state.length - folded);
  state.length += data.size();
  std::size_t const fill = std::min(buffer_size - buffered, data.size());
  std::ranges::copy(data.first(fill), state.buffer.begin() + static_cast<std::ptrdiff_t>(buffered));
  data = data.subspan(fill);
  if(data.empty()) {
    return;
  }
  // The buffer is full and more input follows: fold it, then all but the last 1 to 256 bytes of data.
  std::size_t const whole = ((data.size() - 1) / buffer_size) * buffer_size;
  with_secret(state.seed, [&](secret_array const &secret) {
    fold(state.accumulators, state.buffer, stripe_index(folded), secret, nullptr);
    if(whole != 0) {
      fold(state.accumulators, data.first(whole), stripe_index(folded + buffer_size), secret, nullptr);
    }
  });
  if(whole != 0) {
    std::ranges::copy(data.subspan(whole - stripe_size, stripe_size), state.buffer.end() - stripe_size);
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
}

} // namespace checksum::xxh3_detail

namespace checksum {

template <unsigned Width> constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, std::span<std::byte const> data) noexcept {
  xxh3_detail::update(state, data);
  return state;
}

template <unsigned Width, byte_range Range> constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { xxh3_detail::update(state, chunk); });
  return state;
}

template <unsigned Width> constexpr xxh3_state<Width>::value_type xxh3_finalize(xxh3_state<Width> const &state) noexcept {
  using namespace xxh3_detail;
  if(state.length <= midsize_max) {
    return xxh3_compute<Width>(std::span(state.buffer).first(static_cast<std::size_t>(state.length)), state.seed);
  }
  std::uint64_t const folded = folded_length(state.length);
  auto const buffered = static_cast<std::size_t>(state.length - folded);
  // The last stripe is in the buffer, or wraps around its end.
  std::array<std::byte, stripe_size> wrapped{};
  std::byte const *last_stripe = wrapped.data();
  if(buffered >= stripe_size) {
    last_stripe = state.buffer.data() + buffered - stripe_size;
  } else {
    auto const tail = std::ranges::copy(std::span(state.buffer).last(stripe_size - buffered), wrapped.begin()).out;
    std::ranges::copy(std::span(state.buffer).first(buffered), tail);
  }
  return with_secret(state.seed, [&](secret_array const &secret) {
    accumulator_array accumulators = state.accumulators;
    fold(accumulators, std::span(state.buffer).first(((buffered - 1) / stripe_size) * stripe_size), stripe_index(folded), secret, last_stripe);
    return finish_large<Width>(accumulators, state.length, secret);
  });
}

template <unsigned Width> constexpr xxh3_state<Width>::value_type xxh3_compute(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  if(data.size() <= xxh3_detail::small_max) {
    return xxh3_detail::hash_small<Width>(data, seed);
  }
  if(data.size() <= xxh3_detail::medium_max) {
    return xxh3_detail::hash_medium<Width>(data, seed);
  }
  if(data.size() <= xxh3_detail::midsize_max) {
    return xxh3_detail::hash_midsize<Width>(data, seed);
  }
  return xxh3_detail::hash_large<Width>(data, seed);
}

template <unsigned Width, byte_range Range>
  requires(!std::same_as<std::remove_cvref_t<Range>, std::span<std::byte const>>)
constexpr xxh3_state<Width>::value_type xxh3_compute(Range &&data, std::uint64_t seed) noexcept {
  // Contiguous ranges skip the buffer of the state.
  if constexpr(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>) {
    if !consteval {
      return xxh3_compute<Width>(std::as_bytes(std::span(std::ranges::data(data), std::ranges::size(data))), seed);
    }
  }
  return xxh3_finalize(xxh3_update(xxh3_state<Width>{.seed = seed}, std::forward<Range>(data)));
}

template <byte_range Range> constexpr std::uint64_t xxh3_64_compute(Range &&data, std::uint64_t seed) noexcept {
  return xxh3_compute<64>(std::forward<Range>(data), seed);
}

template <byte_range Range> constexpr hash128 xxh3_128_compute(Range &&data, std::uint64_t seed) noexcept {
  return xxh3_compute<128>(std::forward<Range>(data), seed);
}

} // namespace checksum

#endif // CHECKSUM_XXH3_HPP
