#ifndef CHECKSUM_PRIVATE_FLETCHER_LOOPS_HPP
#define CHECKSUM_PRIVATE_FLETCHER_LOOPS_HPP

// Run-time loops shared by src/fletcher/sum_loop.cpp and src/adler32/sum_loop.cpp: the portable loop over little-endian
// values, the chunk loop of the kernels of fletcher_arch.hpp and the choice between them. Fletcher-N sums values of N/2
// bits modulo 2^(N/2) - 1, Adler-32 bytes modulo 65521.

#include <checksum_private/fletcher_arch.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace checksum::fletcher_detail {

// The two sums of a Fletcher checksum without its block offset: up to 4 bytes pass in one register on 32-bit Arm.
template <class Sum> struct sum_pair {
  Sum sum1 = 0;
  Sum sum2 = 0;
};

// Internal linkage lets compilers inline these loops fully into each source; with external linkage they keep an
// out-of-line copy.
// NOLINTNEXTLINE(misc-anonymous-namespace-in-header): a private header, included only by src/fletcher and src/adler32.
namespace {

#if defined(__aarch64__)
inline constexpr bool narrow_value_sums = true;
#else
inline constexpr bool narrow_value_sums = false;
#endif

// Largest n with (n + 1)(Modulus - 1) + largest_value * n(n + 1) / 2 <= the maximum of Integer: sums below Modulus stay within
// Integer while they add n values of at most largest_value, sum2 after each.
template <class Integer, std::uint64_t Modulus> constexpr std::size_t values_per_reduction(std::uint64_t largest_value) noexcept;

// value modulo Modulus without a division: with k the bit width of Modulus, the bits from k on fold onto the low ones with the
// weight 2^k - Modulus.
template <std::uint64_t Modulus, class Integer> Integer reduce(Integer value) noexcept;

// value modulo Modulus for the kernel chunks. The high half of a 64-bit value folds onto the low one with the weight 2^32 modulo
// Modulus, so that the constant % is a 32-bit multiplication on every target.
template <std::uint64_t Modulus, class Word> Word reduce_chunk(Word value) noexcept;

// The little-endian Value of at most 32 bits at data. Compilers merge the bytes into one load where unaligned loads are allowed,
// and elsewhere load single bytes instead of calling memcpy.
template <class Value> Value load(std::byte const *data) noexcept;

// A sum_pair, adler32_state or fletcher_state with these sums and any other member 0. GCC returns a small aggregate by storing
// its members one by one and reloading them as one word, which stalls store forwarding; a copy of one integer does not.
template <class State> State make_state(std::uint64_t sum1, std::uint64_t sum2) noexcept;

// The loops take and return a sum_pair or an adler32_state, which has the same members: sums below 2 * Modulus in, below
// Modulus out.

// Adds the whole kernel blocks at the start of data to the sums of state, a chunk of at most max_blocks blocks per kernel call, each sum
// reduced after every chunk. Returns the number of bytes taken; inlined into its one caller.
template <class Kernel, class Value, std::uint64_t Modulus, class State>
[[gnu::always_inline]] inline std::size_t sum_chunks(State &state, std::span<std::byte const> data) noexcept;

// The portable loop over the whole Values of data.
template <class Value, std::uint64_t Modulus, class State>
[[gnu::always_inline]] inline State sum_portable(State state, std::span<std::byte const> data) noexcept;

// The CPU kernel, then the portable loop over what it left. Not inlined, so that short inputs run sum_portable() without a call.
// From the minimum size of group_kernel, a tail call to sum_groups() instead.
template <class Value, std::uint64_t Modulus, class State>
[[gnu::noinline, maybe_unused]] State sum_long(State state, std::span<std::byte const> data) noexcept;

// group_kernel, then the portable loop. A separate function, so that its registers cost sum_long() nothing.
template <class Value, std::uint64_t Modulus, class State>
[[gnu::noinline, maybe_unused]] State sum_groups(State state, std::span<std::byte const> data) noexcept;

// Whether size bytes go to sum_long(): from the minimum size of the kernel, if there is one. Callers branch on it themselves,
// so that the call of sum_long() stays a tail call.
template <class Value> constexpr bool runs_kernel(std::size_t size) noexcept;

template <class Integer, std::uint64_t Modulus> constexpr std::size_t values_per_reduction(std::uint64_t largest_value) noexcept {
  constexpr std::uint64_t maximum = std::numeric_limits<Integer>::max();
  std::uint64_t low = 0;
  std::uint64_t high = std::uint64_t{1} << ((std::numeric_limits<Integer>::digits / 2) - 1); // n(n+1) and (n+1)(Modulus-1) cannot overflow
  while(low < high) {
    std::uint64_t const middle = high - ((high - low) / 2);
    std::uint64_t const start = (middle + 1) * (Modulus - 1);
    if(start <= maximum && middle * (middle + 1) / 2 <= (maximum - start) / largest_value) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return static_cast<std::size_t>(low);
}

static_assert(std::numeric_limits<std::size_t>::digits != 64 || values_per_reduction<std::size_t, 255>(255) == 380'368'695);
static_assert(std::numeric_limits<std::size_t>::digits != 64 || values_per_reduction<std::size_t, 65'535>(65'535) == 23'726'745);
static_assert(std::numeric_limits<std::size_t>::digits != 64 || values_per_reduction<std::size_t, 65'521>(255) == 380'368'439);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || values_per_reduction<std::size_t, 255>(255) == 5'802);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || values_per_reduction<std::size_t, 65'535>(65'535) == 360);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || values_per_reduction<std::size_t, 65'521>(255) == 5'552);
static_assert(values_per_reduction<std::uint64_t, 0xFFFF'FFFF>(0xFFFF'FFFF) == 92'680);

template <std::uint64_t Modulus, class Integer> Integer reduce(Integer value) noexcept {
  constexpr int bits = std::bit_width(Modulus);
  if constexpr(bits < std::numeric_limits<Integer>::digits) {
    constexpr Integer low_bits = (Integer{1} << bits) - 1;
    constexpr Integer high_weight = (Integer{1} << bits) - Modulus;
    while((value >> bits) != 0) {
      value = (value & low_bits) + (high_weight * (value >> bits));
    }
  }
  return value >= Modulus ? static_cast<Integer>(value - Modulus) : value;
}

template <std::uint64_t Modulus, class Word> Word reduce_chunk(Word value) noexcept {
  if constexpr(sizeof(Word) > sizeof(std::uint32_t)) {
    constexpr std::uint64_t high_weight = (std::uint64_t{1} << 32U) % Modulus;
    while((value >> 32U) != 0) {
      value = (value & 0xFFFF'FFFFU) + ((value >> 32U) * high_weight);
    }
  }
  return static_cast<std::uint32_t>(value) % static_cast<std::uint32_t>(Modulus);
}

template <class Value> Value load(std::byte const *data) noexcept {
  auto const byte = [data](unsigned index) { return std::to_integer<std::uint32_t>(data[index]) << (8U * index); };
  if constexpr(sizeof(Value) == 1) {
    return std::to_integer<Value>(data[0]);
  } else if constexpr(sizeof(Value) == 2) {
    return static_cast<Value>(byte(0) | byte(1));
  } else {
    static_assert(sizeof(Value) == 4);
    return byte(0) | byte(1) | byte(2) | byte(3);
  }
}

template <class State> State make_state(std::uint64_t sum1, std::uint64_t sum2) noexcept {
  using sum_type = decltype(State::sum1);
  constexpr unsigned bits = std::numeric_limits<sum_type>::digits;
  if constexpr(sizeof(State) <= sizeof(std::size_t) && (std::endian::native == std::endian::little || std::endian::native == std::endian::big)) {
    // The bytes of State from one integer: sum1 at the lowest address, then sum2, the rest 0.
    static_assert(offsetof(State, sum1) == 0 && offsetof(State, sum2) == sizeof(sum_type));
    constexpr std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
    std::uint64_t const low_first = (sum1 & mask) | ((sum2 & mask) << bits);
    std::uint64_t const high_first = ((sum1 & mask) << (64 - bits)) | ((sum2 & mask) << (64 - (2 * bits)));
    std::uint64_t const word = std::endian::native == std::endian::little ? low_first : high_first;
    State state;
    std::memcpy(static_cast<void *>(&state), &word, sizeof(State));
    return state;
  } else {
    return {.sum1 = static_cast<sum_type>(sum1), .sum2 = static_cast<sum_type>(sum2)};
  }
}

template <class Kernel, class Value, std::uint64_t Modulus, class State>
[[gnu::always_inline]] inline std::size_t sum_chunks(State &state, std::span<std::byte const> data) noexcept {
  constexpr unsigned bits = std::numeric_limits<Value>::digits;
  using chunk_kernel = Kernel;
  constexpr std::uint64_t values_per_block = chunk_kernel::block_size / sizeof(Value);
  constexpr std::uint64_t max_values = chunk_kernel::max_blocks * values_per_block;
  constexpr std::uint64_t largest_value = std::numeric_limits<Value>::max();
  constexpr std::uint64_t largest_weighted = max_values * (max_values + 1) / 2 * largest_value;
  constexpr std::uint64_t largest_sum = std::bit_ceil(Modulus) - 1;
  // The weighted sum of a chunk fits 64 bits, and so does sum2 plus max_values * sum1.
  static_assert(max_values * (max_values + 1) / 2 <= std::numeric_limits<std::uint64_t>::max() / largest_value);
  static_assert(max_values <= std::uint64_t{1} << 31U);
  // 32-bit arithmetic where every intermediate result fits, so that 32-bit targets reduce without 64-bit operations.
  constexpr std::uint64_t largest_word = std::numeric_limits<std::uint32_t>::max();
  using word = std::conditional_t<largest_weighted <= largest_word && (max_values + 2) * largest_sum <= largest_word &&
                                      largest_sum + (max_values * largest_value) <= largest_word,
                                  std::uint32_t, std::uint64_t>;
  auto sum1 = static_cast<word>(state.sum1);
  auto sum2 = static_cast<word>(state.sum2);
  std::byte const *position = data.data();
  std::size_t head = 0;
  if constexpr(bits == 8) {
    if constexpr(chunk_kernel::alignment > 1) {
      // Bytes are single blocks, so the bytes before the alignment of the kernel can go first, one by one.
      head = std::min(data.size(), (0 - reinterpret_cast<std::uintptr_t>(position)) & (chunk_kernel::alignment - 1));
      for(std::byte const byte : data.first(head)) {
        sum1 += std::to_integer<word>(byte);
        sum2 += sum1;
      }
      sum1 = reduce_chunk<Modulus>(sum1);
      sum2 = reduce_chunk<Modulus>(sum2);
      position += head;
    }
  }
  std::size_t blocks = (data.size() - head) / chunk_kernel::block_size;
  std::size_t const size = head + (blocks * chunk_kernel::block_size);
  while(blocks != 0) {
    std::size_t const count = std::min<std::size_t>(blocks, chunk_kernel::max_blocks);
    chunk_sums const chunk = chunk_kernel::sum(position, count);
    sum2 =
        reduce_chunk<Modulus>(static_cast<word>(sum2 + (count * values_per_block * sum1) + reduce_chunk<Modulus>(static_cast<word>(chunk.weighted))));
    sum1 = reduce_chunk<Modulus>(static_cast<word>(sum1 + chunk.sum));
    blocks -= count;
    position += count * chunk_kernel::block_size;
  }
  state.sum1 = static_cast<decltype(state.sum1)>(sum1);
  state.sum2 = static_cast<decltype(state.sum2)>(sum2);
  return size;
}

template <class Value, std::uint64_t Modulus, class State>
[[gnu::always_inline]] inline State sum_portable(State state, std::span<std::byte const> data) noexcept {
  // The widest integer the target adds natively, or 64 bits for 32-bit values. AArch64 sums 16-bit values in 32 bits:
  // Fletcher-32 at 20 bytes ran 6 % faster on a Cortex-A72 (a reduction every 360 values costs little), but 9 % slower on x86-64.
  using accumulator =
      std::conditional_t<sizeof(Value) == 4, std::uint64_t, std::conditional_t<sizeof(Value) == 2 && narrow_value_sums, std::uint32_t, std::size_t>>;
  // 32-bit targets sum 16-bit values as 32-bit words, two values each; 64-bit targets keep the 4-value formula on native words.
  constexpr bool sums_words = sizeof(Value) == 2 && std::numeric_limits<std::size_t>::digits == 32;
  constexpr std::size_t size = sizeof(Value);
  constexpr std::size_t run = values_per_reduction<accumulator, Modulus>(std::numeric_limits<Value>::max());
  static_assert(run >= 8);
  // In a run of w words, previous is at most 2M * w(w - 1) / 2.
  static_assert(!sums_words || Modulus * (run / 2) * ((run / 2) - 1) <= std::numeric_limits<std::uint32_t>::max());
  auto sum1 = static_cast<accumulator>(state.sum1 >= Modulus ? state.sum1 - Modulus : state.sum1);
  auto sum2 = static_cast<accumulator>(state.sum2 >= Modulus ? state.sum2 - Modulus : state.sum2);
  std::byte const *position = data.data();
  std::size_t values = data.size() / size;
  while(values != 0) {
    std::size_t count = std::min(values, run);
    values -= count;
    if constexpr(sums_words) {
      // One value first if that aligns the words. A word adds 2 * sum1 + 2 * first half + second half to sum2, so per word
      // previous += sum, sum adds both halves and first_halves the first half.
      if((reinterpret_cast<std::uintptr_t>(position) & 2U) != 0) {
        sum1 += load<Value>(position);
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
      sum2 += (2 * words * sum1) + (2 * static_cast<accumulator>(reduce<Modulus>(previous))) + sum + first_halves;
      sum1 += sum;
    } else {
      // Four values at a time: sum1 has one addition per group in its dependency chain instead of four. With 64-bit sums on a
      // 32-bit target the multiplications cost more than the chain, so the values are added one by one.
      // 64-bit targets bound the loop by its end, which made Fletcher-32 at 20 bytes faster on x86-64; on a Cortex-M4 that cost
      // Fletcher-64 3 % at 4 KiB, so 32-bit targets count the values.
      constexpr bool bounded_by_end = std::numeric_limits<std::size_t>::digits == 64;
      std::byte const *const end = position + ((count / 4) * 4 * size);
      for(; bounded_by_end ? position != end : count >= 4; position += 4 * size, count -= bounded_by_end ? 0 : 4) {
        accumulator const first = load<Value>(position);
        accumulator const second = load<Value>(position + size);
        accumulator const third = load<Value>(position + (2 * size));
        accumulator const fourth = load<Value>(position + (3 * size));
        if constexpr(sizeof(accumulator) > sizeof(std::size_t)) {
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
      count %= 4;
      if constexpr(std::numeric_limits<std::size_t>::digits == 64) {
        if((count & 2U) != 0) {
          accumulator const first = load<Value>(position);
          accumulator const second = load<Value>(position + size);
          sum2 += (2 * sum1) + (2 * first) + second;
          sum1 += first + second;
          position += 2 * size;
        }
        if((count & 1U) != 0) {
          sum1 += load<Value>(position);
          sum2 += sum1;
          position += size;
        }
        count = 0;
      }
    }
    for(; count != 0; --count, position += size) {
      sum1 += load<Value>(position);
      sum2 += sum1;
    }
    if constexpr(sizeof(accumulator) == sizeof(std::uint64_t) && sizeof(std::size_t) == sizeof(std::uint64_t) && std::has_single_bit(Modulus + 1)) {
      // A constant % of a 64-bit sum is a multiplication on 64-bit targets: no branches, which the fold loop mispredicted on
      // the A72 when the number of folds changed with the input size. 32-bit targets would call a division function.
      sum1 %= Modulus;
      sum2 %= Modulus;
    } else {
      sum1 = reduce<Modulus>(sum1);
      sum2 = reduce<Modulus>(sum2);
    }
  }
  return make_state<State>(sum1, sum2);
}

template <class Value, std::uint64_t Modulus, class State>
[[gnu::noinline, maybe_unused]] State sum_long(State state, std::span<std::byte const> data) noexcept {
  constexpr unsigned bits = std::numeric_limits<Value>::digits;
  if constexpr(group_kernel<bits>::available) {
    if(data.size() >= group_kernel<bits>::minimum_size) {
      return sum_groups<Value, Modulus>(state, data);
    }
  }
  std::size_t size = 0;
  if constexpr(kernel<bits>::available) {
    size = sum_chunks<kernel<bits>, Value, Modulus>(state, data);
  }
  return sum_portable<Value, Modulus>(state, data.subspan(size));
}

template <class Value, std::uint64_t Modulus, class State>
[[gnu::noinline, maybe_unused]] State sum_groups(State state, std::span<std::byte const> data) noexcept {
  std::size_t const size = sum_chunks<group_kernel<std::numeric_limits<Value>::digits>, Value, Modulus>(state, data);
  return sum_portable<Value, Modulus>(state, data.subspan(size));
}

template <class Value> constexpr bool runs_kernel(std::size_t size) noexcept {
  using value_kernel = kernel<std::numeric_limits<Value>::digits>;
  return value_kernel::available && size >= value_kernel::minimum_size;
}

} // namespace

} // namespace checksum::fletcher_detail

#endif // CHECKSUM_PRIVATE_FLETCHER_LOOPS_HPP
