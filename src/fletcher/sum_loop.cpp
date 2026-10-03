// Run-time loop of the Fletcher checksums.

#include <checksum/fletcher.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace checksum::fletcher_detail {

namespace {

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

// The widest integer the target adds natively, or 64 bits for the 32-bit blocks of Fletcher-64.
template <unsigned Width> using accumulator = std::conditional_t<Width == 64, std::uint64_t, std::size_t>;

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

// The little-endian block at data.
template <unsigned Width> accumulator<Width> load(std::byte const *data) noexcept {
  typename fletcher_state<Width>::sum_type block = 0;
  std::memcpy(&block, data, sizeof(block));
  if constexpr(std::endian::native == std::endian::big) {
    block = std::byteswap(block);
  }
  return block;
}

template <unsigned Width> fletcher_state<Width> sum_blocks(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  constexpr std::size_t size = block_size<Width>;
  constexpr std::size_t run = blocks_per_reduction<Width>();
  static_assert(run >= 4);
  accumulator<Width> sum1 = state.sum1;
  accumulator<Width> sum2 = state.sum2;
  std::byte const *position = data.data();
  std::size_t blocks = data.size() / size;
  while(blocks != 0) {
    std::size_t count = std::min(blocks, run);
    blocks -= count;
    // Four blocks at a time: sum1 has one addition per group in its dependency chain instead of four.
    for(; count >= 4; count -= 4, position += 4 * size) {
      accumulator<Width> const first = load<Width>(position);
      accumulator<Width> const second = load<Width>(position + size);
      accumulator<Width> const third = load<Width>(position + (2 * size));
      accumulator<Width> const fourth = load<Width>(position + (3 * size));
      sum2 += (4 * sum1) + (4 * first) + (3 * second) + (2 * third) + fourth;
      sum1 += first + second + third + fourth;
    }
    for(; count != 0; --count, position += size) {
      sum1 += load<Width>(position);
      sum2 += sum1;
    }
    sum1 = reduce<Width>(sum1);
    sum2 = reduce<Width>(sum2);
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

} // namespace

fletcher_state<16> sum_loop(fletcher_state<16> state, std::span<std::byte const> data) noexcept { return sum_blocks(state, data); }

fletcher_state<32> sum_loop(fletcher_state<32> state, std::span<std::byte const> data) noexcept { return sum_blocks(state, data); }

fletcher_state<64> sum_loop(fletcher_state<64> state, std::span<std::byte const> data) noexcept { return sum_blocks(state, data); }

} // namespace checksum::fletcher_detail
