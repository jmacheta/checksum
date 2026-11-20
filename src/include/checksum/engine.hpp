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
  Engine const *bound_engine; // never null
  typename Engine::state_type running_state;
};

} // namespace checksum

/// Implementation details shared by the algorithms; not part of the public API.
namespace checksum::detail {

// The Seed of engines without one.
struct no_seed {};

// The static engine Engine with its preset, the last template argument, replaced by Isa.
template <class Engine, isa Isa> struct rebind_preset;

template <template <isa> class Engine, isa Old, isa Isa> struct rebind_preset<Engine<Old>, Isa> {
  using type = Engine<Isa>;
};

template <template <unsigned, isa> class Engine, unsigned Width, isa Old, isa Isa> struct rebind_preset<Engine<Width, Old>, Isa> {
  using type = Engine<Width, Isa>;
};

/// The members of every dispatch engine but CRC's: each runs the static engine Engine rebound to preset(), made with the seed of
/// the dispatch engine if it has one. The preset is chosen once, when the dispatch engine is made, among those that run a loop of
/// their own (isa_detail::runs_own_loop() with KernelPreset).
template <class Engine, isa KernelPreset, class Seed = no_seed> class dispatch_engine {
public:
  using state_type = Engine::state_type; ///< As the static engine's.
  using value_type = Engine::value_type; ///< As the static engine's.

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
    requires requires(Engine const &candidate, value_type value) { candidate.resume(value); };

  /// As the static engine's compute(), with the chosen preset's loop.
  [[nodiscard]] constexpr value_type compute(std::span<std::byte const> data) const noexcept;

  /// As the static engine's compute() for a byte range, with the chosen preset's loop.
  template <byte_range Range> [[nodiscard]] constexpr value_type compute(Range &&data) const noexcept;

protected:
  constexpr explicit dispatch_engine(isa_set cpu, Seed seed = {}) noexcept;

private:
  template <isa Isa> [[nodiscard]] constexpr rebind_preset<Engine, Isa>::type engine() const noexcept;

  Seed hash_seed;
  isa chosen;
};

} // namespace checksum::detail

///@}

namespace checksum {

template <engine Engine>
constexpr accumulator<Engine>::accumulator(Engine const &engine) noexcept : bound_engine(&engine), running_state(engine.initial()) {}

template <engine Engine> constexpr accumulator<Engine> &accumulator<Engine>::update(std::span<std::byte const> data) noexcept {
  running_state = bound_engine->update(running_state, data);
  return *this;
}

template <engine Engine> template <byte_range Range> constexpr accumulator<Engine> &accumulator<Engine>::update(Range &&data) noexcept {
  running_state = bound_engine->update(running_state, std::forward<Range>(data));
  return *this;
}

template <engine Engine>
constexpr accumulator<Engine> &accumulator<Engine>::update(std::span<std::byte const> data, std::size_t bit_length) noexcept
  requires requires(Engine const &candidate, typename Engine::state_type state, std::span<std::byte const> bits) {
    candidate.update(state, bits, std::size_t{});
  }
{
  running_state = bound_engine->update(running_state, data, bit_length);
  return *this;
}

template <engine Engine> constexpr Engine::value_type accumulator<Engine>::value() const noexcept { return bound_engine->finalize(running_state); }

template <engine Engine> constexpr Engine::state_type const &accumulator<Engine>::state() const noexcept { return running_state; }

template <engine Engine> constexpr void accumulator<Engine>::reset() noexcept { running_state = bound_engine->initial(); }

} // namespace checksum

namespace checksum::detail {

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::dispatch_engine(isa_set cpu, Seed seed) noexcept
    : hash_seed(seed), chosen(isa_detail::select(cpu, KernelPreset)) {}

template <class Engine, isa KernelPreset, class Seed>
template <isa Isa>
constexpr rebind_preset<Engine, Isa>::type dispatch_engine<Engine, KernelPreset, Seed>::engine() const noexcept {
  using preset_engine = rebind_preset<Engine, Isa>::type;
  if constexpr(std::same_as<Seed, no_seed>) {
    return preset_engine();
  } else {
    return preset_engine(hash_seed);
  }
}

template <class Engine, isa KernelPreset, class Seed> constexpr isa dispatch_engine<Engine, KernelPreset, Seed>::preset() const noexcept {
  return chosen;
}

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::state_type dispatch_engine<Engine, KernelPreset, Seed>::initial() const noexcept {
  return engine<isa::generic>().initial();
}

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::state_type
dispatch_engine<Engine, KernelPreset, Seed>::update(state_type state, std::span<std::byte const> data) const noexcept {
  return isa_detail::dispatch<KernelPreset>(chosen, [&]<isa Isa>() { return engine<Isa>().update(state, data); });
}

template <class Engine, isa KernelPreset, class Seed>
template <byte_range Range>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::state_type dispatch_engine<Engine, KernelPreset, Seed>::update(state_type state,
                                                                                                                      Range &&data) const noexcept {
  return isa_detail::dispatch<KernelPreset>(chosen, [&]<isa Isa>() { return engine<Isa>().update(state, std::forward<Range>(data)); });
}

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::value_type
dispatch_engine<Engine, KernelPreset, Seed>::finalize(state_type const &state) const noexcept {
  return isa_detail::dispatch<KernelPreset>(chosen, [&]<isa Isa>() { return engine<Isa>().finalize(state); });
}

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::state_type
dispatch_engine<Engine, KernelPreset, Seed>::resume(value_type checksum) const noexcept
  requires requires(Engine const &candidate, value_type value) { candidate.resume(value); }
{
  return engine<isa::generic>().resume(checksum);
}

template <class Engine, isa KernelPreset, class Seed>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::value_type
dispatch_engine<Engine, KernelPreset, Seed>::compute(std::span<std::byte const> data) const noexcept {
  return isa_detail::dispatch<KernelPreset>(chosen, [&]<isa Isa>() { return engine<Isa>().compute(data); });
}

template <class Engine, isa KernelPreset, class Seed>
template <byte_range Range>
constexpr dispatch_engine<Engine, KernelPreset, Seed>::value_type dispatch_engine<Engine, KernelPreset, Seed>::compute(Range &&data) const noexcept {
  return isa_detail::dispatch<KernelPreset>(chosen, [&]<isa Isa>() { return engine<Isa>().compute(std::forward<Range>(data)); });
}

} // namespace checksum::detail

#endif // CHECKSUM_ENGINE_HPP
