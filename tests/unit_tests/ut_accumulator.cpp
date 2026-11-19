// The accumulator with a static and a dispatch engine of every algorithm: splits, value(), reset() and byte ranges.

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

#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using namespace checksum;
using namespace std::literals;

// Each case makes the static engine (generic preset) and the dispatch engine of one algorithm, with a seed where it has one.
struct crc_case {
  static constexpr char const *name = "crc";
  static auto const &make() { return crc_engine_for<crc32, crc_lut_sliced>; }
  static auto make_dispatch() { return crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(crc32).value(); }
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
  static constexpr char const *name = "fletcher";
  static auto make() { return fletcher_engine<Width>{}; }
  static auto make_dispatch() { return fletcher_dispatch_engine<Width>{}; }
};

struct fletcher4_case {
  static constexpr char const *name = "fletcher4";
  static auto make() { return fletcher4_engine{}; }
  static auto make_dispatch() { return fletcher4_dispatch_engine{}; }
};

template <unsigned Width> struct xxhash_case {
  static constexpr char const *name = "xxhash";
  static auto make() { return xxhash_engine<Width>{7}; }
  static auto make_dispatch() { return xxhash_dispatch_engine<Width>{7}; }
};

template <unsigned Width> struct xxh3_case {
  static constexpr char const *name = "xxh3";
  static auto make() { return xxh3_engine<Width>{7}; }
  static auto make_dispatch() { return xxh3_dispatch_engine<Width>{7}; }
};

template <unsigned Width> struct murmur3_case {
  static constexpr char const *name = "murmur3";
  static auto make() { return murmur3_engine<Width>{7}; }
  static auto make_dispatch() { return murmur3_dispatch_engine<Width>{7}; }
};

// Every byte split of a message folded through an accumulator gives compute(), and value() leaves the state unchanged.
template <class Engine> void check_accumulator(Engine const &engine) {
  auto const message = random_bytes(600, 21);
  std::span<std::byte const> const data(message);
  auto const expected = engine.compute(data);
  for(std::size_t split = 0; split <= data.size(); split += 7) {
    accumulator running{engine};
    running.update(data.first(split));
    auto const state = running.state();
    (void)running.value();
    ASSERT_EQ(running.state(), state) << "split " << split;
    running.update(data.subspan(split));
    ASSERT_EQ(running.value(), expected) << "split " << split;
    ASSERT_EQ(running.state(), engine.update(engine.update(engine.initial(), data.first(split)), data.subspan(split))) << "split " << split;
  }
  accumulator running{engine};
  running.update("123"sv).update(std::list<char>{'4', '5', '6', '7', '8', '9'});
  EXPECT_EQ(running.value(), engine.compute("123456789"sv));
  running.reset();
  EXPECT_EQ(running.state(), engine.initial());
  EXPECT_EQ(running.value(), engine.compute(std::span<std::byte const>{}));
}

template <class Case> class accumulator_test : public testing::Test {};

using cases = testing::Types<crc_case, internet_case, adler32_case, fletcher_case<16>, fletcher_case<32>, fletcher_case<64>, fletcher4_case,
                             xxhash_case<32>, xxhash_case<64>, xxh3_case<64>, xxh3_case<128>, murmur3_case<32>, murmur3_case<128>>;

struct case_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Case> static std::string GetName(int index) { return std::string(Case::name) + std::to_string(index); }
};

TYPED_TEST_SUITE(accumulator_test, cases, case_names);

TYPED_TEST(accumulator_test, static_engine) {
  auto const &engine = TypeParam::make();
  check_accumulator(engine);
}

TYPED_TEST(accumulator_test, dispatch_engine) {
  auto const engine = TypeParam::make_dispatch();
  check_accumulator(engine);
}

// The bit-length update exists only for engines that take a bit length.
template <class Engine>
concept updates_bits = requires(accumulator<Engine> running, std::span<std::byte const> data) { running.update(data, std::size_t{}); };
static_assert(updates_bits<crc_engine<std::uint32_t>> && updates_bits<crc_dispatch_engine<std::uint16_t, crc_lut_sliced>>);
static_assert(!updates_bits<internet_engine<>> && !updates_bits<xxh3_dispatch_engine<64>>);

// Accumulators work during constant evaluation and do not bind to temporary engines.
static_assert([] {
  constexpr xxh3_engine<64> engine(3);
  accumulator running{engine};
  running.update("1234"sv).update("56789"sv);
  return running.value() == engine.compute("123456789"sv);
}());
static_assert(!std::is_constructible_v<accumulator<adler32_engine<>>, adler32_engine<> &&>);

} // namespace
