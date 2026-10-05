// Run-time loop of the Fletcher checksums.

#include <checksum/fletcher.hpp>
#include <checksum_private/fletcher_loops.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace checksum::fletcher_detail {

namespace {

template <unsigned Width> using sums_of = sum_pair<typename fletcher_state<Width>::sum_type>;

// The whole blocks of data with the shared loops, from the start of a block.
template <unsigned Width>
[[gnu::always_inline]] inline sums_of<Width> sum_blocks(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// The state word of finished blocks with these sums. sum_pair holds them in the order of the word on little-endian targets,
// so that GCC copies the register instead of taking the sums apart.
template <unsigned Width> state_word<Width> word_of(sums_of<Width> sums) noexcept;

// update_bytes() out of line, so that the byte loop takes no registers from the block loops.
template <unsigned Width> [[gnu::noinline]] fletcher_state<Width> add_bytes(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// Finishes an unfinished block byte by byte, sums the whole blocks, and adds the bytes of a last unfinished block to sum1,
// with the byte loops out of line: for Fletcher-64 and on 32-bit targets, inside sum_any().
template <unsigned Width>
[[gnu::always_inline]] inline fletcher_state<Width> sum_mixed(fletcher_state<Width> state, std::span<std::byte const> data) noexcept;

// The same out of line, with the byte loops inline: Fletcher-16 and -32 on 64-bit targets.
template <unsigned Width> [[gnu::noinline]] state_word<Width> sum_unfinished(state_word<Width> word, std::span<std::byte const> data) noexcept;

// Whole blocks from the start of a block go to sum_blocks() here, everything else to sum_unfinished(), so that this path
// makes no call that needs a frame. Empty data goes there too: knowing data is not empty saves sum_portable() a register.
template <unsigned Width> state_word<Width> sum_any(state_word<Width> word, std::span<std::byte const> data) noexcept;

template <unsigned Width> sums_of<Width> sum_blocks(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  sums_of<Width> const sums{.sum1 = state.sum1, .sum2 = state.sum2};
  return runs_kernel<sum_type>(data.size()) ? sum_long<sum_type, modulus<Width>>(sums, data)
                                            : sum_portable<sum_type, modulus<Width>, kernel_minimum<sum_type>>(sums, data);
}

template <unsigned Width> state_word<Width> word_of(sums_of<Width> sums) noexcept {
  auto const word = std::bit_cast<std::conditional_t<Width == 16, std::uint16_t, std::uint32_t>>(sums);
  return std::endian::native == std::endian::little ? word : std::rotl(word, Width / 2);
}

template <unsigned Width> fletcher_state<Width> add_bytes(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  return update_bytes(state, data);
}

template <unsigned Width> state_word<Width> sum_unfinished(state_word<Width> word, std::span<std::byte const> data) noexcept {
  constexpr unsigned size = block_size<Width>;
  fletcher_state<Width> state = from_word<Width>(word);
  if(state.block_offset % size != 0) {
    std::size_t const head = std::min<std::size_t>(data.size(), size - (state.block_offset % size));
    state = update_bytes(state, data.first(head));
    if(state.block_offset != 0) {
      return to_word(state);
    }
    data = data.subspan(head);
  }
  std::size_t const whole = data.size() - (data.size() % size);
  sums_of<Width> const sums = sum_blocks(state, data.first(whole));
  return to_word(update_bytes(make_state<fletcher_state<Width>>(sums.sum1, sums.sum2), data.subspan(whole)));
}

template <unsigned Width> fletcher_state<Width> sum_mixed(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  constexpr unsigned size = block_size<Width>;
  sum_pair<sum_type> sums{.sum1 = state.sum1, .sum2 = state.sum2};
  if(state.block_offset % size != 0) {
    std::size_t const head = std::min<std::size_t>(data.size(), size - (state.block_offset % size));
    fletcher_state<Width> const finished = add_bytes(state, data.first(head));
    if(finished.block_offset != 0) {
      return finished;
    }
    sums = {.sum1 = finished.sum1, .sum2 = finished.sum2};
    data = data.subspan(head);
  }
  sum_pair<sum_type> const result = runs_kernel<sum_type>(data.size()) ? sum_long<sum_type, modulus<Width>>(sums, data)
                                                                       : sum_portable<sum_type, modulus<Width>, kernel_minimum<sum_type>>(sums, data);
  std::size_t const tail = data.size() % size;
  if(tail == 0) {
    return make_state<fletcher_state<Width>>(result.sum1, result.sum2);
  }
  return add_bytes<Width>({.sum1 = result.sum1, .sum2 = result.sum2, .block_offset = 0}, data.last(tail));
}

template <unsigned Width> state_word<Width> sum_any(state_word<Width> word, std::span<std::byte const> data) noexcept {
  constexpr unsigned size = block_size<Width>;
  if constexpr(Width == 64) {
    // Its state passes as it is; out of line, unfinished blocks cost Fletcher-64 3-8 % on the Cortex-A72.
    return sum_mixed(word, data);
  } else if constexpr(sizeof(std::size_t) < sizeof(std::uint64_t)) {
    // 32-bit targets lost 4-10 % with unfinished blocks out of line.
    return to_word(sum_mixed(from_word<Width>(word), data));
  } else {
    fletcher_state<Width> const state = from_word<Width>(word);
    if(state.block_offset % size != 0 || data.size() % size != 0 || data.empty()) {
      return sum_unfinished<Width>(word, data);
    }
    return word_of<Width>(sum_blocks(state, data));
  }
}

} // namespace

state_word<16> sum_loop(state_word<16> state, std::span<std::byte const> data) noexcept { return sum_any<16>(state, data); }

state_word<32> sum_loop(state_word<32> state, std::span<std::byte const> data) noexcept { return sum_any<32>(state, data); }

state_word<64> sum_loop(state_word<64> state, std::span<std::byte const> data) noexcept { return sum_any<64>(state, data); }

} // namespace checksum::fletcher_detail
