#ifndef CHECKSUM_PRIVATE_XXH3_LOOPS_HPP
#define CHECKSUM_PRIVATE_XXH3_LOOPS_HPP

// The run-time stripe loop of XXH3 for one preset: the CPU kernel of xxh3_arch.hpp and the portable kernel. A source of src/xxh3/
// defines CHECKSUM_PRESET (see target_begin.hpp) and includes this header once.

// Every std header of this file and of the architecture headers comes before the target region, so that their inline functions
// compile for the build's own target.
#include <checksum/isa.hpp>
#include <checksum/xxh3.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <checksum_private/target_begin.hpp>

#include <checksum_private/xxh3_arch.hpp>

namespace checksum::xxh3_detail {

#if CHECKSUM_PRESET_HAS(RISCV_V)
#define CHECKSUM_XXH3_FOLD_TARGET [[gnu::target(CHECKSUM_TARGET_RISCV_V)]]
#else
#define CHECKSUM_XXH3_FOLD_TARGET
#endif

// The stripe loop for a kernel, first declared inside the target region. For RISC-V it carries the vector target attribute.
template <isa Isa, class Kernel>
CHECKSUM_XXH3_FOLD_TARGET void fold_stripes_kernel(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe,
                                                   secret_array const &secret, std::byte const *last_stripe) noexcept;

// The run-time stripe fold for preset Isa; stripe_loop() forwards to it.
template <isa Isa>
CHECKSUM_XXH3_FOLD_TARGET void stripe_any(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe,
                                          secret_array const &secret, std::byte const *last_stripe) noexcept;

template <isa Isa, class Kernel>
CHECKSUM_XXH3_FOLD_TARGET void fold_stripes_kernel(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe,
                                                   secret_array const &secret, std::byte const *last_stripe) noexcept {
  typename Kernel::lanes lanes = Kernel::load(accumulators);
  std::byte const *position = data.data();
  std::byte const *key = secret.data() + (secret_consume_rate * block_stripe);
  std::byte const *const block_end = secret.data() + (secret_consume_rate * stripes_per_block);
  for(std::size_t count = data.size() / stripe_size; count != 0; --count, position += stripe_size) {
    Kernel::accumulate(lanes, position, key);
    key += secret_consume_rate;
    if(key == block_end) {
      Kernel::scramble(lanes, secret.data() + secret_size - stripe_size);
      key = secret.data();
    }
  }
  if(last_stripe != nullptr) {
    Kernel::accumulate(lanes, last_stripe, secret.data() + secret_size - stripe_size - secret_last_accumulate_start);
  }
  Kernel::store(accumulators, lanes);
}

template <isa Isa>
CHECKSUM_XXH3_FOLD_TARGET void stripe_any(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe,
                                          secret_array const &secret, std::byte const *last_stripe) noexcept {
  if constexpr(stripe_kernel<Isa>::available) {
    if(data.size() >= stripe_kernel<Isa>::minimum_size) {
      fold_stripes_kernel<Isa, stripe_kernel<Isa>>(accumulators, data, block_stripe, secret, last_stripe);
      return;
    }
  }
  fold_stripes(accumulators, data, block_stripe, secret, last_stripe);
}

#undef CHECKSUM_XXH3_FOLD_TARGET

} // namespace checksum::xxh3_detail

#include <checksum_private/target_end.hpp>

namespace checksum::xxh3_detail {

// GCC fixes a function's target at its first declaration, which for stripe_loop() is in xxh3.hpp: it only forwards to stripe_any(),
// first declared inside the target region.
template <isa Isa>
void stripe_loop(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe, secret_array const &secret,
                 std::byte const *last_stripe) noexcept {
  stripe_any<Isa>(accumulators, data, block_stripe, secret, last_stripe);
}

template void stripe_loop<isa::CHECKSUM_PRESET>(accumulator_array &accumulators, std::span<std::byte const> data, std::size_t block_stripe,
                                                secret_array const &secret, std::byte const *last_stripe) noexcept;

// The dispatch engines choose this preset exactly where its loop differs from generic's: it has target options or a kernel.
static_assert(own_loops.contains(isa::CHECKSUM_PRESET) ==
              (!isa_detail::without_target_options(isa::CHECKSUM_PRESET) || stripe_kernel<isa::CHECKSUM_PRESET>::available));

} // namespace checksum::xxh3_detail

#endif // CHECKSUM_PRIVATE_XXH3_LOOPS_HPP
