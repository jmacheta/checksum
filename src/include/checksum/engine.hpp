#ifndef CHECKSUM_ENGINE_HPP
#define CHECKSUM_ENGINE_HPP

#include <checksum/byte_range.hpp>
#include <checksum/isa.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <utility>

/**
 * @addtogroup checksum
 * @{
 */

namespace checksum {

/// An engine of any algorithm, static or dispatch: an immutable object that folds data into a separate state_type value and
/// turns a state into the value_type result.
template <class Engine>
concept engine = requires(Engine const &candidate, typename Engine::state_type const &state, std::span<std::byte const> data) {
  { candidate.preset() } -> std::same_as<isa>;
  { candidate.initial() } -> std::same_as<typename Engine::state_type>;
  { candidate.update(state, data) } -> std::same_as<typename Engine::state_type>;
  { candidate.finalize(state) } -> std::same_as<typename Engine::value_type>;
  { candidate.compute(data) } -> std::same_as<typename Engine::value_type>;
};

/// An engine whose result determines the state, so that a computation can continue from a stored result: CRC, Internet,
/// Adler-32, Fletcher and fletcher4.
template <class Engine>
concept resumable_engine = engine<Engine> && requires(Engine const &candidate, typename Engine::value_type value) {
  { candidate.resume(value) } -> std::same_as<typename Engine::state_type>;
};

/// An engine pointer plus a running state, for incremental computations with any engine. The engine must outlive the accumulator.
template <engine Engine> class accumulator {
public:
  /// Starts a computation at engine.initial().
  constexpr explicit accumulator(Engine const &engine) noexcept;

  /// Temporary engines would dangle.
  accumulator(Engine const &&) = delete;

  /// Folds data.
  constexpr accumulator &update(std::span<std::byte const> data) noexcept;

  /// Folds a byte range.
  template <byte_range Range> constexpr accumulator &update(Range &&data) noexcept;

  /// Folds the first bit_length bits of data, for engines that take a bit length (CRC).
  constexpr accumulator &update(std::span<std::byte const> data, std::size_t bit_length) noexcept
    requires requires(Engine const &candidate, typename Engine::state_type state, std::span<std::byte const> bits) {
      candidate.update(state, bits, std::size_t{});
    };

  /// The result of everything folded so far; the state is unchanged.
  [[nodiscard]] constexpr Engine::value_type value() const noexcept;

  /// The running state.
  [[nodiscard]] constexpr Engine::state_type const &state() const noexcept;

  /// Restarts at engine.initial().
  constexpr void reset() noexcept;

private:
  Engine const *engine; // never null
  typename Engine::state_type running_state;
};

} // namespace checksum

/// Implementation details shared by the algorithms; not part of the public API.
namespace checksum::detail {

// The Seed of engines without one.
struct no_seed {};

/// The members of every dispatch engine but CRC's: each runs Engine<preset()>, made with the seed of the dispatch engine if it has
/// one. The preset is chosen once, when the dispatch engine is made.
template <template <isa> class Engine, class Seed = no_seed> class dispatch_engine {
public:
  using state_type = Engine<isa::generic>::state_type; ///< As the static engine's.
  using value_type = Engine<isa::generic>::value_type; ///< As the static engine's.

  /// The preset whose loop the engine runs.
  [[nodiscard]] constexpr isa preset() const noexcept;

  /// As the static engine's initial().
  [[nodiscard]] constexpr state_type initial() const noexcept;

  /// As the static engine's update(), with the chosen preset's loop.
  [[nodiscard]] constexpr state_type update(state_type state, std::span<std::byte const> data) const noexcept;

  /// As the static engine's update() for a byte range, with the chosen preset's loop.
  template <byte_range Range> [[nodiscard]] constexpr state_type update(state_type state, Range &&data) const noexcept;

  /// As the static engine's finalize().
  [[nodiscard]] constexpr value_type finalize(state_type const &state) const noexcept;

  /// As the static engine's resume(), where it has one.
  [[nodiscard]] constexpr state_type resume(value_type checksum) const noexcept
    requires requires(Engine<isa::generic> const &candidate, value_type value) { candidate.resume(value); };

  /// As the static engine's compute(), with the chosen preset's loop.
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// As the static engine's compute() for a byte range, with the chosen preset's loop.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;

protected:
  constexpr explicit dispatch_engine(isa_set cpu, Seed seed = {}) noexcept;

private:
  template <isa Isa> [[nodiscard]] constexpr Engine<Isa> engine() const noexcept;

  Seed seed;
  isa chosen;
};

} // namespace checksum::detail

///@}

namespace checksum {

template <engine Engine>
constexpr accumulator<Engine>::accumulator(Engine const &engine) noexcept : engine(&engine), running_state(engine.initial()) {}

template <engine Engine> constexpr accumulator<Engine> &accumulator<Engine>::update(std::span<std::byte const> data) noexcept {
  running_state = engine->update(running_state, data);
  return *this;
}

template <engine Engine> template <byte_range Range> constexpr accumulator<Engine> &accumulator<Engine>::update(Range &&data) noexcept {
  running_state = engine->update(running_state, std::forward<Range>(data));
  return *this;
}

template <engine Engine>
constexpr accumulator<Engine> &accumulator<Engine>::update(std::span<std::byte const> data, std::size_t bit_length) noexcept
  requires requires(Engine const &candidate, typename Engine::state_type state, std::span<std::byte const> bits) {
    candidate.update(state, bits, std::size_t{});
  }
{
  running_state = engine->update(running_state, data, bit_length);
  return *this;
}

template <engine Engine> constexpr Engine::value_type accumulator<Engine>::value() const noexcept { return engine->finalize(running_state); }

template <engine Engine> constexpr Engine::state_type const &accumulator<Engine>::state() const noexcept { return running_state; }

template <engine Engine> constexpr void accumulator<Engine>::reset() noexcept { running_state = engine->initial(); }

} // namespace checksum

namespace checksum::detail {

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::dispatch_engine(isa_set cpu, Seed seed) noexcept : seed(seed), chosen(isa_detail::select(cpu)) {}

template <template <isa> class Engine, class Seed> template <isa Isa> constexpr Engine<Isa> dispatch_engine<Engine, Seed>::engine() const noexcept {
  if constexpr(std::same_as<Seed, no_seed>) {
    return Engine<Isa>();
  } else {
    return Engine<Isa>(seed);
  }
}

template <template <isa> class Engine, class Seed> constexpr isa dispatch_engine<Engine, Seed>::preset() const noexcept { return chosen; }

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::state_type dispatch_engine<Engine, Seed>::initial() const noexcept {
  return engine<isa::generic>().initial();
}

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::state_type dispatch_engine<Engine, Seed>::update(state_type state,
                                                                                          std::span<std::byte const> data) const noexcept {
  return isa_detail::dispatch(chosen, [&]<isa Isa>() { return engine<Isa>().update(state, data); });
}

template <template <isa> class Engine, class Seed>
template <byte_range Range>
constexpr dispatch_engine<Engine, Seed>::state_type dispatch_engine<Engine, Seed>::update(state_type state, Range &&data) const noexcept {
  return isa_detail::dispatch(chosen, [&]<isa Isa>() { return engine<Isa>().update(state, std::forward<Range>(data)); });
}

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::value_type dispatch_engine<Engine, Seed>::finalize(state_type const &state) const noexcept {
  return isa_detail::dispatch(chosen, [&]<isa Isa>() { return engine<Isa>().finalize(state); });
}

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::state_type dispatch_engine<Engine, Seed>::resume(value_type checksum) const noexcept
  requires requires(Engine<isa::generic> const &candidate, value_type value) { candidate.resume(value); }
{
  return engine<isa::generic>().resume(checksum);
}

template <template <isa> class Engine, class Seed>
constexpr dispatch_engine<Engine, Seed>::value_type dispatch_engine<Engine, Seed>::compute(std::span<std::byte const> data) const noexcept {
  return isa_detail::dispatch(chosen, [&]<isa Isa>() { return engine<Isa>().compute(data); });
}

template <template <isa> class Engine, class Seed>
template <byte_range Range>
constexpr dispatch_engine<Engine, Seed>::value_type dispatch_engine<Engine, Seed>::compute(Range &&data) const noexcept {
  return isa_detail::dispatch(chosen, [&]<isa Isa>() { return engine<Isa>().compute(std::forward<Range>(data)); });
}

} // namespace checksum::detail

#endif // CHECKSUM_ENGINE_HPP
