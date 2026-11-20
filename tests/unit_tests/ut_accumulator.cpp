// The accumulator with a static and a dispatch engine of every algorithm: splits, value(), reset() and byte ranges; for CRC also
// bit-length updates.

#include "crc_test_support.hpp"

#include <checksum/adler32.hpp>
#include <checksum/crc.hpp>
#include <checksum/engine.hpp>
#include <checksum/fletcher.hpp>
#include <checksum/fletcher4.hpp>
#include <checksum/internet.hpp>
#include <checksum/murmur3.hpp>
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using namespace std::literals;
using checksum::adler32_dispatch_engine;
using checksum::adler32_engine;
using checksum::crc_dispatch_engine;
using checksum::crc_engine;
using checksum::crc_engine_for;
using checksum::crc_lut_byte;
using checksum::crc_lut_nibble;
using checksum::crc_lut_none;
using checksum::crc_lut_sliced;
using checksum::fletcher4_dispatch_engine;
using checksum::fletcher4_engine;
using checksum::fletcher_dispatch_engine;
using checksum::fletcher_engine;
using checksum::internet_dispatch_engine;
using checksum::internet_engine;
using checksum::murmur3_dispatch_engine;
using checksum::murmur3_engine;
using checksum::xxh3_dispatch_engine;
using checksum::xxh3_engine;
using checksum::xxhash_dispatch_engine;
using checksum::xxhash_engine;
using crc_test::crc_model;
using crc_test::for_each_engine;
using crc_test::models;

// Each case names one algorithm and width and makes its static engine (generic preset) and dispatch engine, with a seed where it has one.
struct crc_case {
  static constexpr char const *name = "crc";
  static auto const &make() { return crc_engine_for<checksum::crc32, crc_lut_sliced>; }
  static auto make_dispatch() { return crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(checksum::crc32).value(); }
};

struct internet_case {
  static constexpr char const *name = "internet";
  static auto make() { return internet_engine{}; }
  static auto make_dispatch() { return internet_dispatch_engine{}; }
};

struct adler32_case {
  static constexpr char const *name = "adler32";
  static auto make() { return adler32_engine{}; }
  static auto make_dispatch() { return adler32_dispatch_engine{}; }
};

template <unsigned Width> struct fletcher_case {
  static constexpr char const *name = Width == 16 ? "fletcher16" : Width == 32 ? "fletcher32" : "fletcher64";
  static auto make() { return fletcher_engine<Width>{}; }
  static auto make_dispatch() { return fletcher_dispatch_engine<Width>{}; }
};

struct fletcher4_case {
  static constexpr char const *name = "fletcher4";
  static auto make() { return fletcher4_engine{}; }
  static auto make_dispatch() { return fletcher4_dispatch_engine{}; }
};

template <unsigned Width> struct xxhash_case {
  static constexpr char const *name = Width == 32 ? "xxh32" : "xxh64";
  static auto make() { return xxhash_engine<Width>{7}; }
  static auto make_dispatch() { return xxhash_dispatch_engine<Width>{7}; }
};

template <unsigned Width> struct xxh3_case {
  static constexpr char const *name = Width == 64 ? "xxh3_64" : "xxh3_128";
  static auto make() { return xxh3_engine<Width>{7}; }
  static auto make_dispatch() { return xxh3_dispatch_engine<Width>{7}; }
};

template <unsigned Width> struct murmur3_case {
  static constexpr char const *name = Width == 32 ? "murmur3_x86_32" : "murmur3_x64_128";
  static auto make() { return murmur3_engine<Width>{7}; }
  static auto make_dispatch() { return murmur3_dispatch_engine<Width>{7}; }
};

// Every byte split of a message folded through an accumulator gives compute(), and value() leaves the state unchanged.
template <class Engine> void check_accumulator(Engine const &engine) {
  auto const message = random_bytes(600, 21);
  std::span<std::byte const> const data(message);
  auto const expected = engine.compute(data);
  for(std::size_t split = 0; split <= data.size(); split += 7) {
    checksum::accumulator running{engine};
    running.update(data.first(split));
    auto const state = running.state();
    (void)running.value();
    ASSERT_EQ(running.state(), state) << "split " << split;
    running.update(data.subspan(split));
    ASSERT_EQ(running.value(), expected) << "split " << split;
    ASSERT_EQ(running.state(), engine.update(engine.update(engine.initial(), data.first(split)), data.subspan(split))) << "split " << split;
  }
  checksum::accumulator running{engine};
  running.update("123"sv).update(std::list<char>{'4', '5', '6', '7', '8', '9'});
  EXPECT_EQ(running.value(), engine.compute("123456789"sv));
  auto copy = running; // a trivially copyable value
  copy.update("x"sv);
  EXPECT_EQ(running.value(), engine.compute("123456789"sv));
  EXPECT_EQ(copy.value(), engine.compute("123456789x"sv));
  running.reset();
  EXPECT_EQ(running.state(), engine.initial());
  EXPECT_EQ(running.value(), engine.compute(std::span<std::byte const>{}));
}

template <class Case> class accumulator : public testing::Test {};

using cases = testing::Types<crc_case, internet_case, adler32_case, fletcher_case<16>, fletcher_case<32>, fletcher_case<64>, fletcher4_case,
                             xxhash_case<32>, xxhash_case<64>, xxh3_case<64>, xxh3_case<128>, murmur3_case<32>, murmur3_case<128>>;

struct case_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Case> static std::string GetName(int /*index*/) { return Case::name; }
};

TYPED_TEST_SUITE(accumulator, cases, case_names);

TYPED_TEST(accumulator, static_engine) {
  auto const &engine = TypeParam::make();
  static_assert(std::is_trivially_copyable_v<checksum::accumulator<std::remove_cvref_t<decltype(engine)>>>);
  static_assert(!std::is_constructible_v<checksum::accumulator<std::remove_cvref_t<decltype(engine)>>, std::remove_cvref_t<decltype(engine)> &&>);
  check_accumulator(engine);
}

TYPED_TEST(accumulator, dispatch_engine) {
  auto const engine = TypeParam::make_dispatch();
  static_assert(!std::is_constructible_v<checksum::accumulator<std::remove_cvref_t<decltype(engine)>>, std::remove_cvref_t<decltype(engine)> &&>);
  check_accumulator(engine);
}

// The bit-length update exists only for engines that take a bit length.
template <class Engine>
concept updates_bits = requires(checksum::accumulator<Engine> running, std::span<std::byte const> data) { running.update(data, std::size_t{}); };
static_assert(updates_bits<crc_engine<std::uint32_t>> && updates_bits<crc_dispatch_engine<std::uint16_t, crc_lut_sliced>>);
static_assert(!updates_bits<internet_engine<>> && !updates_bits<xxh3_dispatch_engine<64>>);

// Accumulators work during constant evaluation.
static_assert([] {
  constexpr xxh3_engine<64> engine(3);
  checksum::accumulator running{engine};
  running.update("1234"sv).update("56789"sv);
  return running.value() == engine.compute("123456789"sv);
}());

inline constexpr auto crc32 = crc_test::model_parameters<9>; // CRC-32/ISO-HDLC

// CTAD deduces the engine type. The accumulator holds only an engine pointer and a state.
static_assert(std::is_same_v<decltype(checksum::accumulator{crc_engine_for<crc32>}), checksum::accumulator<crc_engine<std::uint32_t, crc_lut_none>>>);
static_assert(std::is_same_v<decltype(checksum::accumulator{crc_engine_for<crc32, crc_lut_nibble>}),
                             checksum::accumulator<crc_engine<std::uint32_t, crc_lut_nibble>>>);
struct pointer_and_state {
  void const *engine;
  std::uint64_t state;
};
static_assert(sizeof(checksum::accumulator<crc_engine<std::uint64_t>>) == sizeof(pointer_and_state));
static_assert(std::is_constructible_v<checksum::accumulator<crc_engine<std::uint32_t>>, crc_engine<std::uint32_t> &>);

inline constexpr checksum::crc_parameters my_crc{.polynomial = 0x8810, .initial_value = 0xFFFF};

// A custom CRC on a nibble table, fed in two chunks.
constexpr std::uint16_t usage_example() {
  checksum::accumulator running{crc_engine_for<my_crc, crc_lut_nibble>};
  running.update("1234"sv).update("56789"sv);
  return running.value();
}
static_assert(usage_example() == 0x29B1); // CRC-16/IBM-3740

// value() leaves the state unchanged and reset() restarts; bit-length updates mix with byte updates.
constexpr bool value_is_non_destructive() {
  auto const &engine = crc_engine_for<crc32, crc_lut_byte>;
  checksum::accumulator running{engine};
  running.update("1234"sv);
  auto const state = running.state();
  (void)running.value();
  bool passed = running.state() == state;
  running.update("56789"sv);
  passed = passed && running.value() == 0xCBF43926 && running.value() == engine.finalize(running.state());
  running.reset();
  passed = passed && running.state() == engine.initial();
  running.update(std::array{std::byte{'1'}}, 8).update(std::array{std::byte{'2'}, std::byte{'3'}}, 16).update("456789"sv);
  return passed && running.value() == 0xCBF43926;
}
static_assert(value_is_non_destructive());

// Every byte split of a 64-byte message, every bit split of its first 64 bits, and bit-by-bit updates equal compute(), for every
// model, strategy and register.
TEST(accumulator, crc_every_split) {
  auto const message = random_bytes(64, 99);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      for(std::size_t split = 0; split <= data.size(); ++split) {
        checksum::accumulator running{engine};
        running.update(data.first(split)).update(data.subspan(split));
        ASSERT_EQ(running.value(), engine.compute(data)) << model.name << " split " << split;
        ASSERT_EQ(running.state(), engine.update(engine.initial(), data));
      }
      for(std::size_t split = 0; split <= 64; ++split) {
        auto const first = crc_test::extract_bits(data, model.reflect_input, 0, split);
        auto const second = crc_test::extract_bits(data, model.reflect_input, split, 64 - split);
        checksum::accumulator running{engine};
        running.update(first, split).update(second, 64 - split);
        ASSERT_EQ(running.value(), engine.compute(data, 64)) << model.name << " bit split " << split;
      }
      checksum::accumulator running{engine};
      for(std::size_t bit = 0; bit < 64; ++bit) {
        running.update(crc_test::extract_bits(data, model.reflect_input, bit, 1), 1);
      }
      EXPECT_EQ(running.value(), engine.compute(data, 64)) << model.name;
    });
  }
}

} // namespace
