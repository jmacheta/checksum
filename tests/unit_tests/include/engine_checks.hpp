#ifndef CHECKSUM_TESTS_ENGINE_CHECKS_HPP
#define CHECKSUM_TESTS_ENGINE_CHECKS_HPP

// Checks shared by the tests of every algorithm, written against the engine concept: incremental updates of one engine, and an
// engine of one preset against the same engine of isa::generic.

#include <checksum/engine.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <list>
#include <span>
#include <string_view>
#include <vector>

namespace engine_checks {

// The result of data, folded in pieces of chunk_size bytes.
template <checksum::engine Engine>
constexpr Engine::value_type chunked(Engine const &engine, std::span<std::byte const> data, std::size_t chunk_size);

// The result of "123456789" folded as "12345678" and "9", for checks during constant evaluation.
template <checksum::engine Engine> constexpr Engine::value_type split_check_text(Engine const &engine);

// Continues the message whose result is checksum with data.
template <checksum::resumable_engine Engine, class Range>
constexpr Engine::value_type continued(Engine const &engine, typename Engine::value_type checksum, Range const &data);

// Every split of data into three updates gives the one-pass result (and state, for resumable engines), and the first two parts
// give the result of their prefix.
template <checksum::engine Engine> void check_split_anywhere(Engine const &engine, std::span<std::byte const> data);

// Every size up to largest (by default 2048 bytes: past every kernel threshold and the first blocks after it), from every start
// offset modulo 8; buffer holds at least largest + 7 bytes.
template <checksum::engine Engine, checksum::engine Generic>
void check_every_size(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer, std::size_t largest = 2048);

// Sizes around 512 and 1024 bytes and up to about 1 MB from four start offsets; buffer holds at least 1'000'006 bytes.
template <checksum::engine Engine, checksum::engine Generic>
void check_large_sizes(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer);

// One state through three updates, split around blocks, buffers and kernel thresholds at four start offsets; then resume(), an
// empty update and a non-contiguous range. buffer holds at least 1307 bytes.
template <checksum::engine Engine, checksum::engine Generic>
void check_updates_and_splits(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer);

// All-ones bytes around the chunk sizes of every kernel (1 KiB to 256 KiB), from the initial state and from start: the inputs that
// come closest to overflowing deferred sums. ones holds at least 512 KiB + 38 bytes.
template <checksum::resumable_engine Engine, checksum::resumable_engine Generic>
void check_all_ones(Engine const &engine, Generic const &generic, std::vector<std::byte> const &ones, typename Engine::state_type const &start);

template <checksum::engine Engine>
constexpr Engine::value_type chunked(Engine const &engine, std::span<std::byte const> data, std::size_t chunk_size) {
  auto state = engine.initial();
  for(; data.size() > chunk_size; data = data.subspan(chunk_size)) {
    state = engine.update(state, data.first(chunk_size));
  }
  return engine.finalize(engine.update(state, data));
}

template <checksum::engine Engine> constexpr Engine::value_type split_check_text(Engine const &engine) {
  return engine.finalize(engine.update(engine.update(engine.initial(), std::string_view("12345678")), std::string_view("9")));
}

template <checksum::resumable_engine Engine, class Range>
constexpr Engine::value_type continued(Engine const &engine, typename Engine::value_type checksum, Range const &data) {
  return engine.finalize(engine.update(engine.resume(checksum), data));
}

template <checksum::engine Engine> void check_split_anywhere(Engine const &engine, std::span<std::byte const> data) {
  auto const whole = engine.compute(data);
  for(std::size_t first = 0; first <= data.size(); ++first) {
    for(std::size_t second = first; second <= data.size(); ++second) {
      auto state = engine.update(engine.initial(), data.first(first));
      state = engine.update(state, data.subspan(first, second - first));
      ASSERT_EQ(engine.finalize(state), engine.compute(data.first(second))) << "split at " << first << " and " << second;
      state = engine.update(state, data.subspan(second));
      ASSERT_EQ(engine.finalize(state), whole) << "split at " << first << " and " << second;
      if constexpr(checksum::resumable_engine<Engine>) {
        ASSERT_EQ(state, engine.update(engine.initial(), data)) << "split at " << first << " and " << second;
      }
    }
  }
}

template <checksum::engine Engine, checksum::engine Generic>
void check_every_size(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer, std::size_t largest) {
  for(std::size_t offset = 0; offset <= 7; ++offset) {
    for(std::size_t size = 0; size <= largest; ++size) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <checksum::engine Engine, checksum::engine Generic>
void check_large_sizes(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer) {
  for(std::size_t const size : {511UZ, 512UZ, 513UZ, 1023UZ, 1024UZ, 1025UZ, 4096UZ, 10'000UZ, 65'536UZ, 100'003UZ, 999'999UZ}) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      ASSERT_EQ(engine.compute(at(buffer, offset, size)), generic.compute(at(buffer, offset, size))) << "offset " << offset << ", size " << size;
    }
  }
}

template <checksum::engine Engine, checksum::engine Generic>
void check_updates_and_splits(Engine const &engine, Generic const &generic, std::vector<std::byte> const &buffer) {
  constexpr std::size_t message_size = 1300;
  constexpr std::array splits{1UZ,   3UZ,   4UZ,   5UZ,   7UZ,   8UZ,   9UZ,   15UZ,  16UZ,  17UZ,  31UZ,  32UZ,  33UZ,  63UZ,  64UZ,   65UZ,
                              127UZ, 128UZ, 129UZ, 191UZ, 192UZ, 193UZ, 240UZ, 241UZ, 255UZ, 256UZ, 257UZ, 511UZ, 512UZ, 513UZ, 1024UZ, 1299UZ};
  for(std::size_t const split : splits) {
    for(std::size_t const offset : {0UZ, 1UZ, 3UZ, 7UZ}) {
      auto const head = at(buffer, offset, split);
      auto const tail = at(buffer, offset + split, message_size - split);
      auto const expected = generic.update(generic.update(generic.initial(), head), tail);
      auto actual = engine.update(engine.initial(), head.first(split / 2));
      actual = engine.update(actual, head.subspan(split / 2));
      actual = engine.update(actual, tail);
      ASSERT_EQ(engine.finalize(actual), generic.finalize(expected)) << "split " << split << ", offset " << offset;
      if constexpr(checksum::resumable_engine<Engine>) {
        ASSERT_EQ(actual, expected) << "split " << split << ", offset " << offset;
      }
    }
  }
  auto const value = generic.compute(buffer);
  if constexpr(checksum::resumable_engine<Engine>) {
    EXPECT_EQ(engine.finalize(engine.update(engine.resume(value), std::span<std::byte const>{})), value);
    EXPECT_EQ(engine.update(engine.resume(value), buffer), generic.update(generic.resume(value), buffer));
  }
  EXPECT_EQ(engine.finalize(engine.update(engine.initial(), std::span<std::byte const>{})), generic.finalize(generic.initial()));
  EXPECT_EQ(engine.compute(buffer), value);
  std::list<std::byte> const list(buffer.begin(), buffer.end());
  EXPECT_EQ(engine.compute(list), value);
}

template <checksum::resumable_engine Engine, checksum::resumable_engine Generic>
void check_all_ones(Engine const &engine, Generic const &generic, std::vector<std::byte> const &ones, typename Engine::state_type const &start) {
  for(std::size_t const chunk : {1024UZ, 2048UZ, 4096UZ, 5600UZ, 5760UZ, 16UZ << 10U, 32UZ << 10U, 256UZ << 10U}) {
    for(std::size_t const size : {chunk - 1, chunk, chunk + 1, chunk + 35, (2 * chunk) + 35}) {
      for(std::size_t const offset : {0UZ, 1UZ, 3UZ}) {
        auto const message = at(ones, offset, size);
        ASSERT_EQ(engine.compute(message), generic.compute(message)) << "size " << size << ", offset " << offset;
        ASSERT_EQ(engine.update(start, message), generic.update(start, message)) << "size " << size << ", offset " << offset;
      }
    }
  }
}

} // namespace engine_checks

#endif // CHECKSUM_TESTS_ENGINE_CHECKS_HPP
