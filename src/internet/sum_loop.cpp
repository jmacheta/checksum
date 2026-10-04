// Run-time loop of the Internet checksum.

#include <checksum/internet.hpp>
#include <checksum_private/internet_arch.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace checksum::internet_detail {

namespace {

// The widest integer the target adds natively. Its carries are worth 2^bits, which is 1 modulo 0xFFFF for any multiple of 16 bits.
using word = std::size_t;

static_assert(std::numeric_limits<word>::digits % 16 == 0);
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

inline constexpr std::size_t word_size = sizeof(word);

// GCC and Clang turn __builtin_addcll into an adc chain on x86-64; GCC 14 on Arm materializes every carry instead.
#if defined(__x86_64__) && defined(__has_builtin)
#if __has_builtin(__builtin_addcll)
inline constexpr bool carry_flag_chain = true;
#else
inline constexpr bool carry_flag_chain = false;
#endif
#else
inline constexpr bool carry_flag_chain = false;
#endif

template <class Integer> Integer load(std::byte const *data) noexcept {
  Integer value = 0;
  std::memcpy(&value, data, sizeof(Integer));
  return value;
}

// sum + value, counting the carry out in carries, or adding it back at once with a carry flag chain.
void add_with_carry(word &sum, word &carries, word value) noexcept;

// Adds Count words from data to sum in one carry chain, counting the carries out.
template <std::size_t Count> [[gnu::always_inline]] inline void add_words(word &sum, word &carries, std::byte const *data) noexcept;

// The portable loop over data, plus a sum of native-order words already taken (unfolded); folded, in native byte order.
[[gnu::always_inline]] inline std::uint16_t sum_words(std::span<std::byte const> data, std::uint64_t blocks) noexcept;

// The CPU kernel, then the portable loop over what it left; swapped like sum_loop(). Not inlined, so that short inputs
// run sum_loop() without a call: a call in it saved registers on every input and cost 13-23 % below 256 bytes on x86-64.
[[gnu::noinline, maybe_unused]] std::uint16_t sum_long(std::span<std::byte const> data) noexcept;

// The sum in native byte order to big-endian order.
std::uint16_t to_big_endian(std::uint16_t sum) noexcept;

void add_with_carry(word &sum, word &carries, word value) noexcept {
  if constexpr(carry_flag_chain) {
    unsigned long long carry = 0;
    sum = __builtin_addcll(sum, value, 0, &carry);
    sum += carry;
  } else {
    sum += value;
    carries += sum < value ? 1U : 0U;
  }
}

template <std::size_t Count> [[gnu::always_inline]] inline void add_words(word &sum, word &carries, std::byte const *data) noexcept {
  if constexpr(carry_flag_chain) {
    // The carry is added back at once: a chain that starts without one leaves a sum below 2^64 - 1 if it carries out.
    unsigned long long carry = 0;
#pragma GCC unroll 8
    for(std::size_t index = 0; index < Count; ++index) {
      sum = __builtin_addcll(sum, load<word>(data + (index * word_size)), carry, &carry);
    }
    sum += carry;
  } else {
#pragma GCC unroll 8
    for(std::size_t index = 0; index < Count; ++index) {
      add_with_carry(sum, carries, load<word>(data + (index * word_size)));
    }
  }
}

[[gnu::always_inline]] inline std::uint16_t sum_words(std::span<std::byte const> data, std::uint64_t blocks) noexcept {
  // Two independent carry chains: one is limited by the latency of add-with-carry.
  std::array<word, 2> sums{};
  std::array<word, 2> carries{};
  std::byte const *position = data.data();
  std::size_t const size = data.size();
  word tail = 0;
  if(size < word_size) {
    // Fixed-size loads at even offsets: a native 32-bit word is congruent to the sum of its 16-bit halves.
    if(word_size > 4 && (size & 4U) != 0) {
      tail += load<std::uint32_t>(position);
      position += 4;
    }
    if(word_size > 2 && (size & 2U) != 0) {
      tail += load<std::uint16_t>(position);
      position += 2;
    }
    if((size & 1U) != 0) {
      // The first byte of a native 16-bit word, padded with zero.
      tail += std::to_integer<word>(*position) << (std::endian::native == std::endian::little ? 0U : 8U);
    }
  } else {
    for(std::byte const *const end = position + (size & ~((8 * word_size) - 1)); position != end; position += 8 * word_size) {
      add_words<4>(sums[0], carries[0], position);
      add_words<4>(sums[1], carries[1], position + (4 * word_size));
    }
    // The rest in fixed steps of 4, 2 and 1 words, then the bytes after the last whole word.
    if((size & (4 * word_size)) != 0) {
      add_words<4>(sums[0], carries[0], position);
      position += 4 * word_size;
    }
    if((size & (2 * word_size)) != 0) {
      add_words<2>(sums[1], carries[1], position);
      position += 2 * word_size;
    }
    if((size & word_size) != 0) {
      add_words<1>(sums[0], carries[0], position);
    }
    if(size % word_size != 0) {
      // The message's last word with the bytes already summed masked off; from an odd offset, rotating it by one byte
      // multiplies it by 2^8, which moves each byte to the other half of its 16-bit word.
      std::size_t const rest_bits = 8 * (size % word_size);
      word const keep = std::endian::native == std::endian::little ? ~(~word{0} >> rest_bits) : ~(~word{0} << rest_bits);
      tail = std::rotr(load<word>(data.data() + size - word_size) & keep, static_cast<int>(8 * (size % 2)));
    }
  }
  // Everything merged into one word, each carry worth 1, then one fold in the word's width; the tail goes into the second
  // chain first, which is usually the shorter one.
  add_with_carry(sums[1], carries[1], tail);
  add_with_carry(sums[1], carries[1], fold(blocks));
  add_with_carry(sums[0], carries[0], sums[1]);
  word last_carry = 0;
  add_with_carry(sums[0], last_carry, carries[0] + carries[1]);
  // After a carry out, sums[0] is below the carries just added, so adding it back cannot overflow.
  return fold(static_cast<word>(sums[0] + last_carry));
}

std::uint16_t to_big_endian(std::uint16_t sum) noexcept { return std::endian::native == std::endian::little ? std::byteswap(sum) : sum; }

[[gnu::noinline, maybe_unused]] std::uint16_t sum_long(std::span<std::byte const> data) noexcept {
  if constexpr(block_sum_available) {
    block_total const total = block_sum(data);
    return to_big_endian(sum_words(data.subspan(total.size), total.sum));
  } else {
    return to_big_endian(sum_words(data, 0));
  }
}

} // namespace

// Words in native byte order: on little-endian targets this sums byte-swapped 16-bit words, and the swap of their sum is the big-endian sum.
std::uint16_t sum_loop(std::span<std::byte const> data) noexcept {
  if constexpr(block_sum_available) {
    if(data.size() >= block_sum_minimum_size) [[unlikely]] {
      return sum_long(data);
    }
  }
  return to_big_endian(sum_words(data, 0));
}

} // namespace checksum::internet_detail
