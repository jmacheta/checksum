// Run-time loop of the Fletcher checksums.

#include <checksum/fletcher.hpp>
#include <checksum_private/fletcher_loops.hpp>

#include <algorithm>
#include <cstddef>
#include <span>

namespace checksum::fletcher_detail {

namespace {

// update_bytes() out of line, so that the byte loop takes no registers from the block loops.
template <unsigned Width> [[gnu::noinline]] fletcher_state<Width> add_bytes(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  return update_bytes(state, data);
}

// Finishes an unfinished block byte by byte, sums the whole blocks with the shared loops, and adds the bytes of a last
// unfinished block to sum1. The byte loop runs only where there are such bytes.
template <unsigned Width> fletcher_state<Width> sum_any(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
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
  sum_pair<sum_type> const result =
      runs_kernel<sum_type>(data.size()) ? sum_long<sum_type, modulus<Width>>(sums, data) : sum_portable<sum_type, modulus<Width>>(sums, data);
  std::size_t const tail = data.size() % size;
  if(tail == 0) {
    return {.sum1 = result.sum1, .sum2 = result.sum2, .block_offset = 0};
  }
  return add_bytes<Width>({.sum1 = result.sum1, .sum2 = result.sum2, .block_offset = 0}, data.last(tail));
}

} // namespace

fletcher_state<16> sum_loop(fletcher_state<16> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<32> sum_loop(fletcher_state<32> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<64> sum_loop(fletcher_state<64> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

} // namespace checksum::fletcher_detail
