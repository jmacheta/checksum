#ifndef CHECKSUM_XXH3_HPP
#define CHECKSUM_XXH3_HPP

#include <checksum/byte_range.hpp>
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

/// A 128-bit XXH3 hash.
struct xxh3_hash128 {
  std::uint64_t low = 0;  ///< The lower 64 bits.
  std::uint64_t high = 0; ///< The upper 64 bits.

  /// Equal if both halves are.
  [[nodiscard]] constexpr bool operator==(xxh3_hash128 const &) const noexcept = default;
};

/// Running XXH3-64 (Width 64) or XXH3-128 (Width 128) hash, 336 bytes. Every value is valid; the default is the empty message with seed 0, and
/// xxh3_state<Width>{.seed = seed} starts one with another seed.
template <unsigned Width>
  requires(Width == 64 || Width == 128)
struct xxh3_state {
  /// The hash type: std::uint64_t or xxh3_hash128.
  using value_type = std::conditional_t<Width == 64, std::uint64_t, xxh3_hash128>;

  /// Lane accumulators of the folded input: all but the last 1 to 256 bytes, rounded down to a multiple of 256.
  std::array<std::uint64_t, 8> accumulators{0xC2B2AE3DU,         0x9E3779B185EBCA87U, 0xC2B2AE3D27D4EB4FU, 0x165667B19E3779F9U,
                                            0x85EBCA77C2B2AE63U, 0x85EBCA77U,         0x27D4EB2F165667C5U, 0x9E3779B1U};
  std::uint64_t length = 0; ///< Bytes passed so far.
  std::uint64_t seed = 0;   ///< Seed of the hash.
  std::array<std::byte, 256>
      buffer{}; ///< The input after the folded part, from the start. While it holds fewer than 64 bytes, its end keeps the stripe before them.
};

/// Folds data into state; data may be split anywhere.
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
inline constexpr std::size_t midsize_max = 240;

// Offsets into the secret, named as in the reference implementation.
inline constexpr std::size_t secret_size_min = 136;
inline constexpr std::size_t midsize_start_offset = 3;
inline constexpr std::size_t midsize_last_offset = 17;
inline constexpr std::size_t secret_consume_rate = 8;
inline constexpr std::size_t secret_last_accumulate_start = 7;
inline constexpr std::size_t secret_merge_start = 11;
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
constexpr xxh3_hash128 multiply_portable(std::uint64_t left, std::uint64_t right) noexcept;

// multiply_portable(), or a native 128-bit multiplication at run time where the compiler has one.
constexpr xxh3_hash128 multiply(std::uint64_t left, std::uint64_t right) noexcept;

// The exclusive or of the two Integer words at secret.
template <class Integer> constexpr Integer secret_xor(std::byte const *secret) noexcept;

// The halves of the full product, combined by exclusive or.
constexpr std::uint64_t multiply_fold(std::uint64_t left, std::uint64_t right) noexcept;

constexpr std::uint64_t avalanche(std::uint64_t value) noexcept;

constexpr std::uint64_t avalanche_xxh64(std::uint64_t value) noexcept;

// The default secret with the seed added to its even words and subtracted from its odd ones.
constexpr secret_array make_secret(std::uint64_t seed) noexcept;

// Mixes 16 bytes of data with 16 bytes of the secret and the seed.
constexpr std::uint64_t mix_step(std::byte const *data, std::byte const *secret, std::uint64_t seed) noexcept;

// Mixes two 16-byte chunks into both halves of a 128-bit accumulator.
constexpr void mix_two_chunks(xxh3_hash128 &accumulator, std::byte const *first, std::byte const *second, std::byte const *secret,
                              std::uint64_t seed) noexcept;

// The hash of 0 to 16 bytes.
template <unsigned Width> constexpr value<Width> hash_small(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// The hash of 17 to 240 bytes.
template <unsigned Width> constexpr value<Width> hash_medium(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// Mixes one stripe into the accumulators.
constexpr void accumulate(accumulator_array &accumulators, std::byte const *stripe, std::byte const *secret) noexcept;

constexpr void scramble(accumulator_array &accumulators, secret_array const &secret) noexcept;

// Folds the whole stripes of data into the accumulators, the first one stripe_index stripes into a block. The accumulators are scrambled
// after the last stripe of each block.
constexpr void fold_stripes(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index,
                            secret_array const &secret) noexcept;

// fold_stripes(), defined in src/xxh3/stripe_loop.cpp.
void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index, secret_array const &secret) noexcept;

// fold_stripes() during constant evaluation, else stripe_loop().
constexpr void fold(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index, secret_array const &secret) noexcept;

// Merges the accumulators into 64 bits with 64 bytes of the secret.
constexpr std::uint64_t merge(accumulator_array const &accumulators, std::byte const *secret, std::uint64_t initial) noexcept;

// The hash of a message of length bytes over 240, from the accumulators of its stripes before the last one and its last 64 bytes.
template <unsigned Width>
constexpr value<Width> finish_large(accumulator_array accumulators, std::byte const *last_stripe, std::uint64_t length,
                                    secret_array const &secret) noexcept;

// The hash of more than 240 bytes.
template <unsigned Width> constexpr value<Width> hash_large(std::span<std::byte const> data, std::uint64_t seed) noexcept;

// Bytes of a message of length bytes that a state has folded into its accumulators.
constexpr std::uint64_t folded_length(std::uint64_t length) noexcept;

// Position in its block of the stripe at offset folded.
constexpr std::size_t stripe_index(std::uint64_t folded) noexcept;

} // namespace checksum::xxh3_detail

///@}
///@}

namespace checksum::xxh3_detail {

constexpr xxh3_hash128 multiply_portable(std::uint64_t left, std::uint64_t right) noexcept {
  std::uint64_t const low_low = (left & low_half_mask) * (right & low_half_mask);
  std::uint64_t const high_low = (left >> 32U) * (right & low_half_mask);
  std::uint64_t const low_high = (left & low_half_mask) * (right >> 32U);
  std::uint64_t const high_high = (left >> 32U) * (right >> 32U);
  std::uint64_t const cross = (low_low >> 32U) + (high_low & low_half_mask) + low_high;
  return {.low = (cross << 32U) | (low_low & low_half_mask), .high = (high_low >> 32U) + (cross >> 32U) + high_high};
}

constexpr xxh3_hash128 multiply(std::uint64_t left, std::uint64_t right) noexcept {
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
  xxh3_hash128 const product = multiply(left, right);
  return product.low ^ product.high;
}

constexpr std::uint64_t avalanche(std::uint64_t value) noexcept {
  value ^= value >> avalanche_shift;
  value *= prime_mx1;
  return value ^ (value >> 32U);
}

constexpr std::uint64_t avalanche_xxh64(std::uint64_t value) noexcept {
  value ^= value >> unsigned{primes_64::avalanche_shifts[0]};
  value *= primes_64::prime_2;
  value ^= value >> unsigned{primes_64::avalanche_shifts[1]};
  value *= primes_64::prime_3;
  return value ^ (value >> unsigned{primes_64::avalanche_shifts[2]});
}

constexpr secret_array make_secret(std::uint64_t seed) noexcept {
  secret_array secret{};
  for(std::size_t index = 0; index < default_secret_words.size(); ++index) {
    std::uint64_t const word = index % 2 == 0 ? default_secret_words[index] + seed : default_secret_words[index] - seed;
    for(std::size_t byte = 0; byte < 8; ++byte) {
      secret[(8 * index) + byte] = static_cast<std::byte>(word >> (8 * byte));
    }
  }
  return secret;
}

inline constexpr secret_array default_secret = make_secret(0);

constexpr std::uint64_t mix_step(std::byte const *data, std::byte const *secret, std::uint64_t seed) noexcept {
  return multiply_fold(load<std::uint64_t>(data) ^ (load<std::uint64_t>(secret) + seed),
                       load<std::uint64_t>(data + 8) ^ (load<std::uint64_t>(secret + 8) - seed));
}

constexpr void mix_two_chunks(xxh3_hash128 &accumulator, std::byte const *first, std::byte const *second, std::byte const *secret,
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
      xxh3_hash128 product = multiply(mixed_first, primes_64::prime_1);
      product.low += (length - 1) << length_shift;
      product.high += mixed_last + ((mixed_last & low_half_mask) * (primes_32::prime_2 - 1));
      product.low ^= std::byteswap(product.high);
      xxh3_hash128 const result = multiply(product.low, primes_64::prime_2);
      return {.low = avalanche(result.low), .high = avalanche(result.high + (product.high * primes_64::prime_2))};
    }
  }
  if(length >= 4) {
    std::uint64_t const first = load<std::uint32_t>(input);
    std::uint64_t const last = load<std::uint32_t>(input + length - 4);
    std::uint64_t const modified_seed = seed ^ (std::uint64_t{std::byteswap(static_cast<std::uint32_t>(seed))} << 32U);
    if constexpr(Width == 64) {
      std::uint64_t value = (secret_xor<std::uint64_t>(secret + 8) - modified_seed) ^ (last | (first << 32U));
      value ^= std::rotl(value, mix_rotations[0]) ^ std::rotl(value, mix_rotations[1]);
      value *= prime_mx2;
      value ^= (value >> mix_shifts[0]) + length;
      value *= prime_mx2;
      return value ^ (value >> mix_shifts[1]);
    } else {
      std::uint64_t const value = (secret_xor<std::uint64_t>(secret + 16) + modified_seed) ^ (first | (last << 32U));
      xxh3_hash128 const product = multiply(value, primes_64::prime_1 + (length << 2U));
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
      return avalanche_xxh64(low);
    } else {
      std::uint64_t const high = (std::uint64_t{secret_xor<std::uint32_t>(secret + 8)} - seed) ^ std::rotl(std::byteswap(combined), 13);
      return {.low = avalanche_xxh64(low), .high = avalanche_xxh64(high)};
    }
  }
  if constexpr(Width == 64) {
    constexpr std::size_t offset = 56;
    return avalanche_xxh64(seed ^ secret_xor<std::uint64_t>(secret + offset));
  } else {
    return {.low = avalanche_xxh64(seed ^ secret_xor<std::uint64_t>(secret + 64)),
            .high = avalanche_xxh64(seed ^ secret_xor<std::uint64_t>(secret + 64 + 16))};
  }
}

template <unsigned Width> constexpr value<Width> hash_medium(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  std::byte const *input = data.data();
  std::byte const *secret = default_secret.data();
  std::size_t const length = data.size();
  std::size_t const rounds = ((length - 1) / 32) + 1;
  if constexpr(Width == 64) {
    std::uint64_t accumulator = length * primes_64::prime_1;
    if(length <= 128) {
      for(std::size_t round = rounds; round-- != 0;) {
        accumulator += mix_step(input + (16 * round), secret + (32 * round), seed);
        accumulator += mix_step(input + length - (16 * round) - 16, secret + (32 * round) + 16, seed);
      }
    } else {
      for(std::size_t chunk = 0; chunk < 8; ++chunk) {
        accumulator += mix_step(input + (16 * chunk), secret + (16 * chunk), seed);
      }
      accumulator = avalanche(accumulator);
      for(std::size_t chunk = 8; chunk < length / 16; ++chunk) {
        accumulator += mix_step(input + (16 * chunk), secret + (16 * (chunk - 8)) + midsize_start_offset, seed);
      }
      accumulator += mix_step(input + length - 16, secret + secret_size_min - midsize_last_offset, seed);
    }
    return avalanche(accumulator);
  } else {
    xxh3_hash128 accumulator{.low = length * primes_64::prime_1, .high = 0};
    if(length <= 128) {
      for(std::size_t round = rounds; round-- != 0;) {
        mix_two_chunks(accumulator, input + (16 * round), input + length - (16 * round) - 16, secret + (32 * round), seed);
      }
    } else {
      for(std::size_t chunk = 0; chunk < 4; ++chunk) {
        mix_two_chunks(accumulator, input + (32 * chunk), input + (32 * chunk) + 16, secret + (32 * chunk), seed);
      }
      accumulator = {.low = avalanche(accumulator.low), .high = avalanche(accumulator.high)};
      for(std::size_t chunk = 4; chunk < length / 32; ++chunk) {
        mix_two_chunks(accumulator, input + (32 * chunk), input + (32 * chunk) + 16, secret + (32 * (chunk - 4)) + midsize_start_offset, seed);
      }
      mix_two_chunks(accumulator, input + length - 16, input + length - 32, secret + secret_size_min - midsize_last_offset - 16, 0 - seed);
    }
    std::uint64_t const high =
        (accumulator.low * primes_64::prime_1) + (accumulator.high * primes_64::prime_4) + ((length - seed) * primes_64::prime_2);
    return {.low = avalanche(accumulator.low + accumulator.high), .high = 0 - avalanche(high)};
  }
}

constexpr void accumulate(accumulator_array &accumulators, std::byte const *stripe, std::byte const *secret) noexcept {
  for(std::size_t lane = 0; lane < accumulators.size(); ++lane) {
    auto const word = load<std::uint64_t>(stripe + (8 * lane));
    std::uint64_t const key = word ^ load<std::uint64_t>(secret + (8 * lane));
    accumulators[lane ^ 1U] += word;
    accumulators[lane] += (key & low_half_mask) * (key >> 32U);
  }
}

constexpr void scramble(accumulator_array &accumulators, secret_array const &secret) noexcept {
  for(std::size_t lane = 0; lane < accumulators.size(); ++lane) {
    std::uint64_t const accumulator = accumulators[lane] ^ (accumulators[lane] >> scramble_shift);
    accumulators[lane] = (accumulator ^ load<std::uint64_t>(secret.data() + secret_size - stripe_size + (8 * lane))) * primes_32::prime_1;
  }
}

constexpr void fold_stripes(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index,
                            secret_array const &secret) noexcept {
  // A local copy stays in registers: stores through the reference could alias data.
  accumulator_array lanes = accumulators;
  std::byte const *position = data.data();
  for(std::size_t count = data.size() / stripe_size; count != 0; --count, position += stripe_size) {
    accumulate(lanes, position, secret.data() + (secret_consume_rate * stripe_index));
    if(++stripe_index == stripes_per_block) {
      scramble(lanes, secret);
      stripe_index = 0;
    }
  }
  accumulators = lanes;
}

constexpr void fold(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t stripe_index, secret_array const &secret) noexcept {
  if consteval {
    fold_stripes(accumulators, data, stripe_index, secret);
  } else {
    stripe_loop(accumulators, data, stripe_index, secret);
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
constexpr value<Width> finish_large(accumulator_array accumulators, std::byte const *last_stripe, std::uint64_t length,
                                    secret_array const &secret) noexcept {
  accumulate(accumulators, last_stripe, secret.data() + secret_size - stripe_size - secret_last_accumulate_start);
  std::uint64_t const low = merge(accumulators, secret.data() + secret_merge_start, length * primes_64::prime_1);
  if constexpr(Width == 64) {
    return low;
  } else {
    return {.low = low, .high = merge(accumulators, secret.data() + secret_size - stripe_size - secret_merge_start, ~(length * primes_64::prime_2))};
  }
}

template <unsigned Width> constexpr value<Width> hash_large(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  secret_array const secret = make_secret(seed);
  accumulator_array accumulators = initial_accumulators;
  fold(accumulators, data.first(((data.size() - 1) / stripe_size) * stripe_size), 0, secret);
  return finish_large<Width>(accumulators, data.data() + data.size() - stripe_size, data.size(), secret);
}

constexpr std::uint64_t folded_length(std::uint64_t length) noexcept { return length == 0 ? 0 : ((length - 1) / buffer_size) * buffer_size; }

constexpr std::size_t stripe_index(std::uint64_t folded) noexcept { return static_cast<std::size_t>((folded / stripe_size) % stripes_per_block); }

} // namespace checksum::xxh3_detail

namespace checksum {

template <unsigned Width> constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, std::span<std::byte const> data) noexcept {
  using namespace xxh3_detail;
  std::uint64_t const folded = folded_length(state.length);
  auto const buffered = static_cast<std::size_t>(state.length - folded);
  state.length += data.size();
  std::size_t const fill = std::min(buffer_size - buffered, data.size());
  std::ranges::copy(data.first(fill), state.buffer.begin() + static_cast<std::ptrdiff_t>(buffered));
  data = data.subspan(fill);
  if(data.empty()) {
    return state;
  }
  // The buffer is full and more input follows: fold it, then all but the last 1 to 256 bytes of data.
  secret_array const secret = make_secret(state.seed);
  fold(state.accumulators, state.buffer, stripe_index(folded), secret);
  std::size_t const whole = ((data.size() - 1) / buffer_size) * buffer_size;
  if(whole != 0) {
    fold(state.accumulators, data.first(whole), stripe_index(folded + buffer_size), secret);
    std::ranges::copy(data.subspan(whole - stripe_size, stripe_size), state.buffer.end() - stripe_size);
  }
  std::ranges::copy(data.subspan(whole), state.buffer.begin());
  return state;
}

template <unsigned Width, byte_range Range> constexpr xxh3_state<Width> xxh3_update(xxh3_state<Width> state, Range &&data) noexcept {
  detail::for_each_chunk(std::forward<Range>(data), [&](std::span<std::byte const> chunk) { state = xxh3_update(state, chunk); });
  return state;
}

template <unsigned Width> constexpr xxh3_state<Width>::value_type xxh3_finalize(xxh3_state<Width> const &state) noexcept {
  using namespace xxh3_detail;
  if(state.length <= midsize_max) {
    return xxh3_compute<Width>(std::span(state.buffer).first(static_cast<std::size_t>(state.length)), state.seed);
  }
  std::uint64_t const folded = folded_length(state.length);
  auto const buffered = static_cast<std::size_t>(state.length - folded);
  secret_array const secret = make_secret(state.seed);
  accumulator_array accumulators = state.accumulators;
  fold(accumulators, std::span(state.buffer).first(((buffered - 1) / stripe_size) * stripe_size), stripe_index(folded), secret);
  std::array<std::byte, stripe_size> last_stripe{};
  if(buffered >= stripe_size) {
    std::ranges::copy(std::span(state.buffer).subspan(buffered - stripe_size, stripe_size), last_stripe.begin());
  } else {
    auto const tail = std::ranges::copy(std::span(state.buffer).last(stripe_size - buffered), last_stripe.begin()).out;
    std::ranges::copy(std::span(state.buffer).first(buffered), tail);
  }
  return finish_large<Width>(accumulators, last_stripe.data(), state.length, secret);
}

template <unsigned Width> constexpr xxh3_state<Width>::value_type xxh3_compute(std::span<std::byte const> data, std::uint64_t seed) noexcept {
  if(data.size() <= xxh3_detail::small_max) {
    return xxh3_detail::hash_small<Width>(data, seed);
  }
  if(data.size() <= xxh3_detail::midsize_max) {
    return xxh3_detail::hash_medium<Width>(data, seed);
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

} // namespace checksum

#endif // CHECKSUM_XXH3_HPP
