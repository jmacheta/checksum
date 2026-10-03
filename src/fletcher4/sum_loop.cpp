// Run-time loop of the fletcher4 checksum.

#include <checksum/fletcher4.hpp>
#include <checksum_private/fletcher4_arch.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace checksum::fletcher4_detail {

namespace {

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

// Bytes of one word per lane.
inline constexpr std::size_t group_size = lane_count * word_size;

// Shortest input for the lanes. The portable ones overtake the word loop on a Cortex-A72 at 256 bytes with GCC, 384 with Clang; on 32-bit targets
// their 64-bit sums run out of registers, 3 times slower there.
inline constexpr std::size_t lanes_minimum_size =
    lane_kernel_available ? lane_kernel_minimum_size : (sizeof(std::size_t) == 8 ? 256 : std::numeric_limits<std::size_t>::max());

// The little-endian word at data.
std::uint64_t load(std::byte const *data) noexcept;

// weights[sum][source][lane]: the weight of the lane's source sum in that sum of the words in message order. Word i of n has the weights 1,
// n-i, C(n-i+1, 2) and C(n-i+2, 3) in sum1 to sum4, which these weights express through the lane sums.
constexpr std::array<std::array<std::array<std::int64_t, lane_count>, 4>, 4> weights{{
    {{{1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {{{0, -1, -2, -3}, {4, 4, 4, 4}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {{{0, 0, 1, 3}, {-6, -10, -14, -18}, {16, 16, 16, 16}, {0, 0, 0, 0}}},
    {{{0, 0, 0, -1}, {4, 10, 20, 34}, {-48, -64, -80, -96}, {64, 64, 64, 64}}},
}};

// The portable lanes over groups groups of words at data.
[[gnu::always_inline, maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept;

// The state after the words that lanes summed, a multiple of lane_count; the earlier sums add to the later ones once per word.
fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept;

// One word at a time, then the bytes of an unfinished word.
[[gnu::always_inline]] inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept;

// The lanes of the CPU kernel, else the portable ones, then sum_words() over what they left. Not inlined, so that short inputs
// run sum_loop() without a call.
[[gnu::noinline]] fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept;

std::uint64_t load(std::byte const *data) noexcept {
  std::uint32_t word = 0;
  std::memcpy(&word, data, sizeof(word));
  if constexpr(std::endian::native == std::endian::big) {
    word = std::byteswap(word);
  }
  return word;
}

[[gnu::always_inline, maybe_unused]] inline lane_sums sum_lanes(std::byte const *data, std::size_t groups) noexcept {
  lane_sums lanes{};
  for(; groups != 0; --groups, data += group_size) {
    for(std::size_t lane = 0; lane < lane_count; ++lane) {
      lanes[0][lane] += load(data + (lane * word_size));
      lanes[1][lane] += lanes[0][lane];
      lanes[2][lane] += lanes[1][lane];
      lanes[3][lane] += lanes[2][lane];
    }
  }
  return lanes;
}

fletcher4_state combine(fletcher4_state state, lane_sums const &lanes, std::uint64_t words) noexcept {
  // C(n+1, 2) and C(n+2, 3) modulo 2^64 for an even n; the second divides exactly by 3, a product with its inverse modulo 2^64.
  std::uint64_t const triangular = (words / 2) * (words + 1);
  std::uint64_t const tetrahedral = triangular * (words + 2) * 0xAAAA'AAAA'AAAA'AAABU;
  std::array<std::uint64_t, 4> sums{state.sum1, state.sum2 + (words * state.sum1), state.sum3 + (words * state.sum2) + (triangular * state.sum1),
                                    state.sum4 + (words * state.sum3) + (triangular * state.sum2) + (tetrahedral * state.sum1)};
  for(std::size_t sum = 0; sum < sums.size(); ++sum) {
    for(std::size_t source = 0; source <= sum; ++source) {
      for(std::size_t lane = 0; lane < lane_count; ++lane) {
        sums[sum] += static_cast<std::uint64_t>(weights[sum][source][lane]) * lanes[source][lane];
      }
    }
  }
  return {.sum1 = sums[0], .sum2 = sums[1], .sum3 = sums[2], .sum4 = sums[3]};
}

[[gnu::always_inline]] inline fletcher4_state sum_words(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::byte const *position = data.data();
  for(std::size_t words = data.size() / word_size; words != 0; --words, position += word_size) {
    state.sum1 += load(position);
    finish_word(state);
  }
  // The bytes of an unfinished word, at most 3, go to sum1 only.
  std::size_t const tail = data.size() % word_size;
  for(std::size_t index = 0; index < tail; ++index) {
    state.sum1 += std::to_integer<std::uint64_t>(position[index]) << (8 * index);
  }
  state.word_offset = static_cast<std::uint8_t>(tail);
  return state;
}

[[gnu::noinline]] fletcher4_state sum_long(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::size_t const groups = data.size() / group_size;
  if constexpr(lane_kernel_available) {
    state = combine(state, lane_kernel(data.data(), groups), groups * lane_count);
  } else {
    state = combine(state, sum_lanes(data.data(), groups), groups * lane_count);
  }
  return sum_words(state, data.subspan(groups * group_size));
}

} // namespace

fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept {
  return data.size() >= lanes_minimum_size ? sum_long(state, data) : sum_words(state, data);
}

} // namespace checksum::fletcher4_detail
