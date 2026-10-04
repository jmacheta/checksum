// Run-time loop of the Adler-32 checksum.

#include <checksum/adler32.hpp>
#include <checksum_private/fletcher_arch.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace checksum::adler32_detail {

namespace {

// Largest n with (n+1)(M-1) + 255n(n+1)/2 <= the maximum of std::size_t. Sums start at most at M-1, and n bytes of at most 255 raise sum1 to
// at most M-1 + 255n and sum2 to at most (n+1)(M-1) + 255n(n+1)/2.
constexpr std::size_t bytes_per_reduction() noexcept {
  constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
  std::size_t low = 0;
  std::size_t high = std::size_t{1} << ((sizeof(std::size_t) * 4U) - 1U); // n(n+1) and (n+1)(M-1) cannot overflow
  while(low < high) {
    std::size_t const middle = high - ((high - low) / 2);
    std::size_t const start = (middle + 1) * (modulus - 1);
    if(middle * (middle + 1) / 2 <= (maximum - start) / 255) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return low;
}

static_assert(std::numeric_limits<std::size_t>::digits != 64 || bytes_per_reduction() == 380'368'439);
static_assert(std::numeric_limits<std::size_t>::digits != 32 || bytes_per_reduction() == 5'552);

// value modulo 65521, without a division: 2^16 is 15 modulo 65521, so the high bits fold onto the low ones.
template <class Integer> Integer reduce(Integer value) noexcept;

// The portable loop.
[[gnu::always_inline]] inline adler32_state sum_bytes(adler32_state state, std::span<std::byte const> data) noexcept;

// The byte kernel of Fletcher-16, then the portable loop over what it left. Not inlined, so that short inputs run sum_loop()
// without a call.
[[gnu::noinline, maybe_unused]] adler32_state sum_long(adler32_state state, std::span<std::byte const> data) noexcept;

using byte_kernel = fletcher_detail::kernel<8>;

template <class Integer> Integer reduce(Integer value) noexcept {
  while(value > 0xFFFF) {
    value = (value & 0xFFFF) + (15 * (value >> 16U));
  }
  return reduce_once(static_cast<std::uint32_t>(value));
}

[[gnu::always_inline]] inline adler32_state sum_bytes(adler32_state state, std::span<std::byte const> data) noexcept {
  constexpr std::size_t run = bytes_per_reduction();
  static_assert(run >= 4);
  std::size_t sum1 = reduce_once(state.sum1);
  std::size_t sum2 = reduce_once(state.sum2);
  std::byte const *position = data.data();
  std::size_t remaining = data.size();
  while(remaining != 0) {
    std::size_t count = std::min(remaining, run);
    remaining -= count;
    // Four bytes at a time: sum1 has one addition per group in its dependency chain instead of four.
    for(; count >= 4; count -= 4, position += 4) {
      std::size_t const first = std::to_integer<std::size_t>(position[0]);
      std::size_t const second = std::to_integer<std::size_t>(position[1]);
      std::size_t const third = std::to_integer<std::size_t>(position[2]);
      std::size_t const fourth = std::to_integer<std::size_t>(position[3]);
      sum2 += (4 * sum1) + (4 * first) + (3 * second) + (2 * third) + fourth;
      sum1 += first + second + third + fourth;
    }
    for(; count != 0; --count, ++position) {
      sum1 += std::to_integer<std::size_t>(*position);
      sum2 += sum1;
    }
    sum1 = reduce(sum1);
    sum2 = reduce(sum2);
  }
  return {.sum1 = static_cast<std::uint16_t>(sum1), .sum2 = static_cast<std::uint16_t>(sum2)};
}

[[gnu::noinline, maybe_unused]] adler32_state sum_long(adler32_state state, std::span<std::byte const> data) noexcept {
  if constexpr(byte_kernel::available) {
    fletcher_detail::sum_pair sums{.sum1 = state.sum1, .sum2 = state.sum2};
    std::size_t const size = fletcher_detail::sum_chunks<8, modulus>(sums, data);
    return sum_bytes({.sum1 = static_cast<std::uint16_t>(sums.sum1), .sum2 = static_cast<std::uint16_t>(sums.sum2)}, data.subspan(size));
  } else {
    return sum_bytes(state, data);
  }
}

} // namespace

adler32_state sum_loop(adler32_state state, std::span<std::byte const> data) noexcept {
  if constexpr(byte_kernel::available) {
    return data.size() >= byte_kernel::minimum_size ? sum_long(state, data) : sum_bytes(state, data);
  } else {
    return sum_bytes(state, data);
  }
}

} // namespace checksum::adler32_detail
