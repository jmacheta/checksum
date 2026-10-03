// Run-time loop of the Fletcher checksums.

#include <checksum/fletcher.hpp>
#include <checksum_private/fletcher_arch.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace checksum::fletcher_detail {

namespace {

// The widest integer the target adds natively, or 64 bits for the 32-bit blocks of Fletcher-64.
template <unsigned Width> using accumulator = std::conditional_t<Width == 64, std::uint64_t, std::size_t>;

// Fletcher-32 on 32-bit targets sums 32-bit words, two blocks each; 64-bit targets keep the 4-block formula on native words.
template <unsigned Width> constexpr bool sums_words = Width == 32 && std::numeric_limits<std::size_t>::digits == 32;

// Largest n with M * (1 + n(n+3)/2) <= the accumulator's maximum. Sums start at most at M, and n blocks of at most M raise sum1 to at most
// (n+1)M and sum2 to at most M(1 + n(n+3)/2).
template <unsigned Width> constexpr std::size_t blocks_per_reduction() noexcept {
  using integer = accumulator<Width>;
  constexpr std::uint64_t quotient = std::numeric_limits<integer>::max() / modulus<Width>;
  constexpr std::uint64_t limit = quotient - 1;
  integer low = 0;
  integer high = integer{1} << ((sizeof(integer) * 4U) - 1U); // n(n+3) cannot overflow
  while(low < high) {
    integer const middle = high - ((high - low) / 2);
    if(middle * (middle + 3) / 2 <= limit) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return static_cast<std::size_t>(low);
}

static_assert(std::numeric_limits<std::size_t>::digits != 64 || blocks_per_reduction<16>() == 380'368'695);
static_assert(std::numeric_limits<std::size_t>::digits != 64 || blocks_per_reduction<32>() == 23'726'745);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || blocks_per_reduction<16>() == 5'802);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || blocks_per_reduction<32>() == 360);
static_assert(blocks_per_reduction<64>() == 92'680);

// The little-endian Integer of at most 32 bits at data. Compilers merge the bytes into one load where unaligned loads are
// allowed, and elsewhere load single bytes instead of calling memcpy.
template <class Integer> Integer load(std::byte const *data) noexcept;

// The portable loop.
template <unsigned Width>
[[gnu::always_inline]] inline fletcher_state<Width> sum_blocks(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// The CPU kernel, then the portable loop over what it left. Not inlined, so that short inputs run sum_any() without a call.
template <unsigned Width>
[[gnu::noinline, maybe_unused]] fletcher_state<Width> sum_long(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// The kernel from its minimum size, else the portable loop.
template <unsigned Width> fletcher_state<Width> sum_any(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

template <class Integer> Integer load(std::byte const *data) noexcept {
  auto const byte = [data](unsigned index) { return std::to_integer<std::uint32_t>(data[index]) << (8U * index); };
  if constexpr(sizeof(Integer) == 1) {
    return std::to_integer<Integer>(data[0]);
  } else if constexpr(sizeof(Integer) == 2) {
    return static_cast<Integer>(byte(0) | byte(1));
  } else {
    static_assert(sizeof(Integer) == 4);
    return byte(0) | byte(1) | byte(2) | byte(3);
  }
}

template <unsigned Width>
[[gnu::always_inline]] inline fletcher_state<Width> sum_blocks(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  constexpr std::size_t size = block_size<Width>;
  constexpr std::size_t run = blocks_per_reduction<Width>();
  static_assert(run >= 8);
  // In a run of w words, previous is at most 2M * w(w - 1) / 2.
  static_assert(!sums_words<Width> || modulus<Width> * (run / 2) * ((run / 2) - 1) <= std::numeric_limits<std::uint32_t>::max());
  accumulator<Width> sum1 = state.sum1;
  accumulator<Width> sum2 = state.sum2;
  std::byte const *position = data.data();
  std::size_t blocks = data.size() / size;
  while(blocks != 0) {
    std::size_t count = std::min(blocks, run);
    blocks -= count;
    if constexpr(sums_words<Width>) {
      // One block first if that aligns the words. A word adds 2 * sum1 + 2 * first half + second half to sum2, so per word
      // previous += sum, sum adds both halves and first_halves the first half.
      if((reinterpret_cast<std::uintptr_t>(position) & 2U) != 0) {
        sum1 += load<sum_type>(position);
        sum2 += sum1;
        position += size;
        --count;
      }
      std::size_t const words = (count / 8) * 4;
      std::uint32_t sum = 0;
      std::uint32_t previous = 0;
      std::uint32_t first_halves = 0;
      for(; count >= 8; count -= 8, position += 16) {
        auto const first = load<std::uint32_t>(position);
        auto const second = load<std::uint32_t>(position + 4);
        auto const third = load<std::uint32_t>(position + 8);
        auto const fourth = load<std::uint32_t>(position + 12);
        previous += sum;
        sum += (first & 0xFFFFU) + (first >> 16U);
        first_halves += first & 0xFFFFU;
        previous += sum;
        sum += (second & 0xFFFFU) + (second >> 16U);
        first_halves += second & 0xFFFFU;
        previous += sum;
        sum += (third & 0xFFFFU) + (third >> 16U);
        first_halves += third & 0xFFFFU;
        previous += sum;
        sum += (fourth & 0xFFFFU) + (fourth >> 16U);
        first_halves += fourth & 0xFFFFU;
      }
      // With previous reduced, sum2 stays below 1300 * M.
      sum2 += (2 * words * sum1) + (2 * static_cast<accumulator<Width>>(reduce<Width>(previous))) + sum + first_halves;
      sum1 += sum;
    } else {
      // Four blocks at a time: sum1 has one addition per group in its dependency chain instead of four. With 64-bit sums on a
      // 32-bit target the multiplications cost more than the chain, so the blocks are added one by one.
      for(; count >= 4; count -= 4, position += 4 * size) {
        accumulator<Width> const first = load<sum_type>(position);
        accumulator<Width> const second = load<sum_type>(position + size);
        accumulator<Width> const third = load<sum_type>(position + (2 * size));
        accumulator<Width> const fourth = load<sum_type>(position + (3 * size));
        if constexpr(sizeof(accumulator<Width>) > sizeof(std::size_t)) {
          sum1 += first;
          sum2 += sum1;
          sum1 += second;
          sum2 += sum1;
          sum1 += third;
          sum2 += sum1;
          sum1 += fourth;
          sum2 += sum1;
        } else {
          sum2 += (4 * sum1) + (4 * first) + (3 * second) + (2 * third) + fourth;
          sum1 += first + second + third + fourth;
        }
      }
    }
    for(; count != 0; --count, position += size) {
      sum1 += load<sum_type>(position);
      sum2 += sum1;
    }
    sum1 = static_cast<accumulator<Width>>(reduce<Width>(sum1));
    sum2 = static_cast<accumulator<Width>>(reduce<Width>(sum2));
  }
  // The bytes of an unfinished block, at most 3, go to sum1 only.
  std::size_t const tail = data.size() % size;
  for(std::size_t index = 0; index < tail; ++index) {
    sum1 += std::to_integer<accumulator<Width>>(position[index]) << (8 * index);
  }
  return {.sum1 = static_cast<sum_type>(reduce<Width>(sum1)),
          .sum2 = static_cast<sum_type>(reduce<Width>(sum2)),
          .block_offset = static_cast<std::uint8_t>(tail)};
}

template <unsigned Width>
[[gnu::noinline, maybe_unused]] fletcher_state<Width> sum_long(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  sum_pair sums{.sum1 = state.sum1, .sum2 = state.sum2};
  std::size_t const size = sum_chunks<Width / 2, modulus<Width>>(sums, data);
  state = {.sum1 = static_cast<sum_type>(sums.sum1), .sum2 = static_cast<sum_type>(sums.sum2), .block_offset = 0};
  return sum_blocks(state, data.subspan(size));
}

template <unsigned Width> fletcher_state<Width> sum_any(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  if constexpr(kernel<Width / 2>::available) {
    return data.size() >= kernel<Width / 2>::minimum_size ? sum_long(state, data) : sum_blocks(state, data);
  } else {
    return sum_blocks(state, data);
  }
}

} // namespace

fletcher_state<16> sum_loop(fletcher_state<16> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<32> sum_loop(fletcher_state<32> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<64> sum_loop(fletcher_state<64> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

} // namespace checksum::fletcher_detail
