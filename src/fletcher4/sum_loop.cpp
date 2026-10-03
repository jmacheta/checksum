// Run-time loop of the fletcher4 checksum.

#include <checksum/fletcher4.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace checksum::fletcher4_detail {

namespace {

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

// Interleaved lanes: lane j folds the words j, j + 4, j + 8, ... as a message of its own.
inline constexpr std::size_t lane_count = 4;

// The little-endian word at data.
std::uint64_t load(std::byte const *data) noexcept {
  std::uint32_t word = 0;
  std::memcpy(&word, data, sizeof(word));
  if constexpr(std::endian::native == std::endian::big) {
    word = std::byteswap(word);
  }
  return word;
}

// n(n+1)/2 and n(n+1)(n+2)/6 modulo 2^64: the factor divisible by 2, and the one divisible by 3, are divided before the product wraps.
std::uint64_t triangular(std::uint64_t count) noexcept {
  std::array<std::uint64_t, 2> factors{count, count + 1};
  factors[count % 2] /= 2;
  return factors[0] * factors[1];
}

std::uint64_t tetrahedral(std::uint64_t count) noexcept {
  std::array<std::uint64_t, 3> factors{count, count + 1, count + 2};
  factors[(3 - (count % 3)) % 3] /= 3;
  factors[count % 2] /= 2; // Dividing by 3 keeps the parity.
  return factors[0] * factors[1] * factors[2];
}

// weights[sum][source][lane]: the weight of the lane's source sum in that sum of the words in message order. Word i of n has the weights 1,
// n-i, C(n-i+1, 2) and C(n-i+2, 3) in sum1 to sum4, which these weights express through the lane sums.
constexpr std::array<std::array<std::array<std::int64_t, lane_count>, 4>, 4> weights{{
    {{{1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {{{0, -1, -2, -3}, {4, 4, 4, 4}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {{{0, 0, 1, 3}, {-6, -10, -14, -18}, {16, 16, 16, 16}, {0, 0, 0, 0}}},
    {{{0, 0, 0, -1}, {4, 10, 20, 34}, {-48, -64, -80, -96}, {64, 64, 64, 64}}},
}};

// The state after words more words at data, a multiple of lane_count; the earlier sums add to the later ones once per word.
fletcher4_state sum_lanes(fletcher4_state state, std::byte const *data, std::uint64_t words) noexcept {
  // lanes[source][lane], so that one sum of all lanes is contiguous for vector instructions.
  std::array<std::array<std::uint64_t, lane_count>, 4> lanes{};
  for(std::uint64_t remaining = words; remaining != 0; remaining -= lane_count, data += lane_count * word_size) {
    for(std::size_t lane = 0; lane < lane_count; ++lane) {
      lanes[0][lane] += load(data + (lane * word_size));
      lanes[1][lane] += lanes[0][lane];
      lanes[2][lane] += lanes[1][lane];
      lanes[3][lane] += lanes[2][lane];
    }
  }
  std::array<std::uint64_t, 4> sums{state.sum1, state.sum2 + (words * state.sum1),
                                    state.sum3 + (words * state.sum2) + (triangular(words) * state.sum1),
                                    state.sum4 + (words * state.sum3) + (triangular(words) * state.sum2) + (tetrahedral(words) * state.sum1)};
  for(std::size_t sum = 0; sum < sums.size(); ++sum) {
    for(std::size_t source = 0; source <= sum; ++source) {
      for(std::size_t lane = 0; lane < lane_count; ++lane) {
        sums[sum] += static_cast<std::uint64_t>(weights[sum][source][lane]) * lanes[source][lane];
      }
    }
  }
  return {.sum1 = sums[0], .sum2 = sums[1], .sum3 = sums[2], .sum4 = sums[3]};
}

} // namespace

fletcher4_state sum_loop(fletcher4_state state, std::span<std::byte const> data) noexcept {
  std::byte const *position = data.data();
  std::size_t words = data.size() / word_size;
  // Independent lanes overlap their dependency chains; the combination costs a few dozen operations.
  std::size_t const grouped = words - (words % lane_count);
  if(grouped >= 32) {
    state = sum_lanes(state, position, grouped);
    position += grouped * word_size;
    words -= grouped;
  }
  for(; words != 0; --words, position += word_size) {
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

} // namespace checksum::fletcher4_detail
