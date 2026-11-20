#ifndef CHECKSUM_PRIVATE_INTERNET_LOOPS_HPP
#define CHECKSUM_PRIVATE_INTERNET_LOOPS_HPP

// The run-time loop of the Internet checksum for one preset: the portable loop over native-order words, the CPU kernel of
// internet_arch.hpp and the choice between them. A source of src/internet/ defines CHECKSUM_PRESET (see target_begin.hpp)
// and includes this header once, which compiles sum_loop<isa::CHECKSUM_PRESET>().

// Every std header of this file and of the architecture headers comes before the target region, so that their inline
// functions compile for the build's own target.
#include <checksum/internet.hpp>
#include <checksum/isa.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include <checksum_private/target_begin.hpp>

#include <checksum_private/internet_arch.hpp>

namespace checksum::internet_detail {

static_assert(std::numeric_limits<word>::digits % 16 == 0);
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

inline constexpr std::size_t word_size = sizeof(word);

// Every function below is a template on the preset, so that each preset's copy has its own name.
template <isa Isa, class Integer> Integer load(std::byte const *data) noexcept;

// sum + value, counting the carry out in carries, or adding it back at once with a carry flag chain.
template <isa Isa> void add_with_carry(word &sum, word &carries, word value) noexcept;

// Adds Count words from data to sum in one carry chain, counting the carries out.
template <isa Isa, std::size_t Count> CHECKSUM_ALWAYS_INLINE inline void add_words(word &sum, word &carries, std::byte const *data) noexcept;

// The portable loop over data, plus a sum of native-order words already taken (unfolded); folded, in native byte order.
template <isa Isa> CHECKSUM_ALWAYS_INLINE inline std::uint16_t sum_words(std::span<std::byte const> data, std::uint64_t blocks) noexcept;

// The CPU kernel, then the portable loop over what it left; swapped like sum_any(). Not inlined, so that short inputs
// run the entry without a call: a call in it saved registers on every input and cost 13-23 % below 256 bytes on x86-64.
template <isa Isa> CHECKSUM_NOINLINE std::uint16_t sum_long(std::span<std::byte const> data) noexcept;

// The sum in native byte order to big-endian order.
template <isa Isa> std::uint16_t to_big_endian(std::uint16_t sum) noexcept;

// The run-time sum of data, in big-endian order, compiled for Isa; sum_loop() forwards to it.
template <isa Isa> std::uint16_t sum_any(std::span<std::byte const> data) noexcept;

template <isa Isa, class Integer> Integer load(std::byte const *data) noexcept {
  Integer value = 0;
  std::memcpy(&value, data, sizeof(Integer));
  return value;
}

template <isa Isa> void add_with_carry(word &sum, word &carries, word value) noexcept {
  if constexpr(carry_chain<Isa>::available) {
    word carry = 0;
    sum = carry_chain<Isa>::add(sum, value, carry);
    sum += carry;
  } else {
    sum += value;
    carries += sum < value ? 1U : 0U;
  }
}

template <isa Isa, std::size_t Count> CHECKSUM_ALWAYS_INLINE inline void add_words(word &sum, word &carries, std::byte const *data) noexcept {
  // A carry flag chain adds its carry back at once: one that starts without a carry leaves a sum below the word's maximum if
  // it carries out.
  word carry = 0;
#if defined(__GNUC__)
#pragma GCC unroll 8
#endif
  for(std::size_t index = 0; index < Count; ++index) {
    if constexpr(carry_chain<Isa>::available) {
      sum = carry_chain<Isa>::add(sum, load<Isa, word>(data + (index * word_size)), carry);
    } else {
      add_with_carry<Isa>(sum, carries, load<Isa, word>(data + (index * word_size)));
    }
  }
  sum += carry;
}

template <isa Isa> CHECKSUM_ALWAYS_INLINE inline std::uint16_t sum_words(std::span<std::byte const> data, std::uint64_t blocks) noexcept {
  // Two independent carry chains: one is limited by the latency of add-with-carry.
  std::array<word, 2> sums{};
  std::array<word, 2> carries{};
  std::byte const *position = data.data();
  std::size_t const size = data.size();
  word tail = 0;
  if(size < word_size) {
    // Fixed-size loads at even offsets: a native 32-bit word is congruent to the sum of its 16-bit halves.
    if(word_size > 4 && (size & 4U) != 0) {
      tail += load<Isa, std::uint32_t>(position);
      position += 4;
    }
    if(word_size > 2 && (size & 2U) != 0) {
      tail += load<Isa, std::uint16_t>(position);
      position += 2;
    }
    if((size & 1U) != 0) {
      // The first byte of a native 16-bit word, padded with zero.
      tail += std::to_integer<word>(*position) << (std::endian::native == std::endian::little ? 0U : 8U);
    }
  } else {
    for(std::byte const *const end = position + (size & ~((8 * word_size) - 1)); position != end; position += 8 * word_size) {
      add_words<Isa, 4>(sums[0], carries[0], position);
      add_words<Isa, 4>(sums[1], carries[1], position + (4 * word_size));
    }
    // The rest in fixed steps of 4, 2 and 1 words, then the bytes after the last whole word.
    if((size & (4 * word_size)) != 0) {
      add_words<Isa, 4>(sums[0], carries[0], position);
      position += 4 * word_size;
    }
    if((size & (2 * word_size)) != 0) {
      add_words<Isa, 2>(sums[1], carries[1], position);
      position += 2 * word_size;
    }
    if((size & word_size) != 0) {
      add_words<Isa, 1>(sums[0], carries[0], position);
    }
    if(size % word_size != 0) {
      // The message's last word with the bytes already summed masked off; from an odd offset, rotating it by one byte
      // multiplies it by 2^8, which moves each byte to the other half of its 16-bit word.
      std::size_t const rest_bits = 8 * (size % word_size);
      word const keep = std::endian::native == std::endian::little ? ~(~word{0} >> rest_bits) : ~(~word{0} << rest_bits);
      tail = std::rotr(load<Isa, word>(data.data() + size - word_size) & keep, static_cast<int>(8 * (size % 2)));
    }
  }
  // Everything merged into one word, each carry worth 1, then one fold in the word's width; the tail goes into the second
  // chain first, which is usually the shorter one.
  add_with_carry<Isa>(sums[1], carries[1], tail);
  add_with_carry<Isa>(sums[1], carries[1], fold(blocks));
  add_with_carry<Isa>(sums[0], carries[0], sums[1]);
  word last_carry = 0;
  add_with_carry<Isa>(sums[0], last_carry, carries[0] + carries[1]);
  // After a carry out, sums[0] is below the carries just added, so adding it back cannot overflow.
  return fold(static_cast<word>(sums[0] + last_carry));
}

template <isa Isa> std::uint16_t to_big_endian(std::uint16_t sum) noexcept {
  return std::endian::native == std::endian::little ? std::byteswap(sum) : sum;
}

template <isa Isa> CHECKSUM_NOINLINE std::uint16_t sum_long(std::span<std::byte const> data) noexcept {
  block_total const total = block_kernel<Isa>::sum(data);
  return to_big_endian<Isa>(sum_words<Isa>(data.subspan(total.size), total.sum));
}

// Words in native byte order: on little-endian targets this sums byte-swapped 16-bit words, and the swap of their sum is
// the big-endian sum.
template <isa Isa> std::uint16_t sum_any(std::span<std::byte const> data) noexcept {
  if constexpr(block_kernel<Isa>::available) {
    if(data.size() >= block_kernel<Isa>::minimum_size) [[unlikely]] {
      return sum_long<Isa>(data);
    }
  }
  return to_big_endian<Isa>(sum_words<Isa>(data, 0));
}

} // namespace checksum::internet_detail

#include <checksum_private/target_end.hpp>

namespace checksum::internet_detail {

// GCC fixes a function's target at its first declaration, which for sum_loop() is in internet.hpp: it only forwards to
// sum_any(), first declared inside the target region.
template <isa Isa> std::uint16_t sum_loop(std::span<std::byte const> data) noexcept { return sum_any<Isa>(data); }

template std::uint16_t sum_loop<isa::CHECKSUM_PRESET>(std::span<std::byte const> data) noexcept;

// The dispatch engines choose this preset exactly where its loop differs from generic's: it has target options or a kernel.
static_assert(own_loops.contains(isa::CHECKSUM_PRESET) ==
              (!isa_detail::without_target_options(isa::CHECKSUM_PRESET) || block_kernel<isa::CHECKSUM_PRESET>::available));

} // namespace checksum::internet_detail

#endif // CHECKSUM_PRIVATE_INTERNET_LOOPS_HPP
