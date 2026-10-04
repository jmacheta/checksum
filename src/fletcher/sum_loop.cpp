// Run-time loop of the Fletcher checksums.

#include <checksum/fletcher.hpp>
#include <checksum_private/fletcher_loops.hpp>

#include <algorithm>
#include <cstddef>
#include <span>

namespace checksum::fletcher_detail {

namespace {

// Finishes an unfinished block byte by byte, sums the whole blocks with the shared loops, and adds the bytes of a last
// unfinished block to sum1.
template <unsigned Width> fletcher_state<Width> sum_any(fletcher_state<Width> state, std::span<std::byte const> data) noexcept {
  using sum_type = fletcher_state<Width>::sum_type;
  constexpr unsigned size = block_size<Width>;
  std::size_t const head = std::min<std::size_t>(data.size(), (size - (state.block_offset % size)) % size);
  state = update_bytes(state, data.first(head));
  data = data.subspan(head);
  sum_pair const sums = sum_values<sum_type, modulus<Width>>({.sum1 = state.sum1, .sum2 = state.sum2}, data);
  state = {.sum1 = static_cast<sum_type>(sums.sum1), .sum2 = static_cast<sum_type>(sums.sum2), .block_offset = state.block_offset};
  return update_bytes(state, data.last(data.size() % size));
}

} // namespace

fletcher_state<16> sum_loop(fletcher_state<16> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<32> sum_loop(fletcher_state<32> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

fletcher_state<64> sum_loop(fletcher_state<64> state, std::span<std::byte const> data) noexcept { return sum_any(state, data); }

} // namespace checksum::fletcher_detail
