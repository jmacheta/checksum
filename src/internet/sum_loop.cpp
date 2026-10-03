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

template <class Integer> Integer load(std::byte const *data) noexcept {
  Integer value = 0;
  std::memcpy(&value, data, sizeof(Integer));
  return value;
}

// sum + value, counting the carry out.
void add_with_carry(word &sum, word &carries, word value) noexcept {
  sum += value;
  carries += sum < value ? 1U : 0U;
}

} // namespace

// Words in native byte order: on little-endian targets this sums byte-swapped 16-bit words, and the swap of their sum is the big-endian sum.
std::uint16_t sum_loop(std::span<std::byte const> data) noexcept {
  // Two independent carry chains: one is limited by the latency of add-with-carry.
  std::array<word, 2> sums{};
  std::array<word, 2> carries{};
  std::uint64_t vector = 0;
  if constexpr(vector_sum_available) {
    if(data.size() >= vector_sum_minimum_size) {
      vector = vector_sum(data);
    }
  }
  std::byte const *position = data.data();
  std::size_t size = data.size();
  for(; size >= 4 * word_size; size -= 4 * word_size, position += 4 * word_size) {
    add_with_carry(sums[0], carries[0], load<word>(position));
    add_with_carry(sums[1], carries[1], load<word>(position + word_size));
    add_with_carry(sums[0], carries[0], load<word>(position + (2 * word_size)));
    add_with_carry(sums[1], carries[1], load<word>(position + (3 * word_size)));
  }
  for(; size >= word_size; size -= word_size, position += word_size) {
    add_with_carry(sums[0], carries[0], load<word>(position));
  }
  // Fixed-size loads at even offsets: a native 32-bit word is congruent to the sum of its 16-bit halves.
  std::uint64_t tail = 0;
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
    tail += std::to_integer<std::uint64_t>(*position) << (std::endian::native == std::endian::little ? 0U : 8U);
  }
  // One chain left, folded once. tail + vector fits word: kernels exist only for 64-bit words, and tail is below 2^17.
  add_with_carry(sums[0], carries[0], sums[1]);
  add_with_carry(sums[0], carries[0], static_cast<word>(tail + vector));
  std::uint16_t const sum = fold(std::uint64_t{fold(sums[0])} + carries[0] + carries[1]);
  return std::endian::native == std::endian::little ? std::byteswap(sum) : sum;
}

} // namespace checksum::internet_detail
