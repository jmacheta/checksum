// CRC presets: every preset this build has must give the results of isa::generic, at run time and at compile time. crc_lut_sliced
// and crc_lut_braided must match crc_lut_byte for every catalog set, on the smallest register and std::uint64_t, one-shot, split and
// through an accumulator; the first set of each code path runs every length, the others the lengths around each threshold. The other
// table strategies run three common sets at every length 0..300.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/isa.hpp>

#include "crc_catalog_checks.hpp"
#include "crc_test_support.hpp"

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace checksum;

constexpr std::size_t longest = 4096;
constexpr std::size_t common_length = 300;

// Aligned and misaligned for every load width; no CRC loop branches on the address.
constexpr std::array<std::size_t, 2> offsets{0, 15};

// Every length 0..640 (two steps of the widest loop, 256 bytes, with every tail), then random lengths up to longest.
std::vector<std::size_t> every_length() {
  constexpr std::size_t exhaustive = 640;
  constexpr std::size_t random_count = 24;
  std::vector<std::size_t> result;
  for(std::size_t length = 0; length <= exhaustive; ++length) {
    result.push_back(length);
  }
  std::mt19937_64 generator(0xACCE1);
  std::uniform_int_distribution<std::size_t> distribution(exhaustive + 1, longest);
  for(std::size_t index = 0; index < random_count; ++index) {
    result.push_back(distribution(generator));
  }
  result.push_back(longest);
  return result;
}

// Every length up to 80 (the 8-byte words, CRC32 steps, 16-byte blocks, 40-byte braids and 64-byte steps), then each
// longer threshold ±1.
std::vector<std::size_t> threshold_lengths() {
  std::vector<std::size_t> result;
  for(std::size_t length = 0; length <= 80; ++length) {
    result.push_back(length);
  }
  for(std::size_t const size : {128U, 256U, 512U, 1024U}) {
    result.insert(result.end(), {size - 1, size, size + 1});
  }
  result.push_back(longest);
  return result;
}

// Parameter sets with the same register, bit order and (for CRC32 instructions) polynomial run the same code.
struct code_path {
  unsigned register_bits;
  bool reflected;
  std::uint64_t polynomial;

  bool operator==(code_path const &) const = default;
};

code_path code_path_of(crc_parameters const &parameters) {
  unsigned const width = parameters.polynomial.width();
  unsigned const register_bits = width <= 8 ? 8 : width <= 16 ? 16 : width <= 32 ? 32 : 64;
  return {.register_bits = register_bits,
          .reflected = parameters.reflect_input,
          .polynomial = register_bits == 32 && parameters.reflect_input ? parameters.polynomial.normal_form() : 0};
}

template <class Strategy, isa Preset, class Register>
void check_against_byte(crc_parameters const &parameters, std::span<std::byte const> message, std::span<std::size_t const> lengths) {
  // On the heap: a braided engine holds 32 KiB of tables.
  auto const reference =
      std::make_unique<crc_engine<Register, crc_lut_byte, isa::generic>>(*crc_engine<Register, crc_lut_byte, isa::generic>::create(parameters));
  auto const smallest = std::make_unique<crc_engine<Register, Strategy, Preset>>(*crc_engine<Register, Strategy, Preset>::create(parameters));
  auto const wide = std::make_unique<crc_engine<std::uint64_t, Strategy, Preset>>(*crc_engine<std::uint64_t, Strategy, Preset>::create(parameters));
  for(std::size_t const length : lengths) {
    for(std::size_t const offset : offsets) {
      auto const data = message.subspan(offset, length);
      auto const expected = reference->compute(data);
      ASSERT_EQ(smallest->compute(data), expected) << "length " << length << ", offset " << offset;
      ASSERT_EQ(wide->compute(data), expected) << "length " << length << ", offset " << offset;
      std::size_t const split = length / 3;
      std::size_t const second_split = (2 * length) / 3;
      ASSERT_EQ(smallest->finalize(smallest->update(smallest->update(smallest->initial(), data.first(split)), data.subspan(split))), expected)
          << "length " << length << ", offset " << offset << ", split " << split;
      ASSERT_EQ(wide->finalize(wide->update(wide->update(wide->initial(), data.first(split)), data.subspan(split))), expected)
          << "length " << length << ", offset " << offset << ", split " << split;
      // Accumulators in three parts, so that a kernel can start and stop in the middle of a message.
      accumulator<crc_engine<Register, Strategy, Preset>> running(*smallest);
      running.update(data.first(split)).update(data.subspan(split, second_split - split)).update(data.subspan(second_split));
      ASSERT_EQ(running.value(), expected) << "length " << length << ", offset " << offset;
      accumulator<crc_engine<std::uint64_t, Strategy, Preset>> wide_accumulator(*wide);
      wide_accumulator.update(data.first(second_split)).update(data.subspan(second_split));
      ASSERT_EQ(wide_accumulator.value(), expected) << "length " << length << ", offset " << offset;
    }
  }
  accumulator<crc_engine<Register, Strategy, Preset>> running(*smallest);
  running.update(message.first(lengths.back()));
  running.reset();
  running.update(message.first(lengths.back()));
  EXPECT_EQ(running.value(), reference->compute(message.first(lengths.back())));
}

template <class Strategy, isa Preset>
void check_set(crc_parameters const &parameters, std::span<std::byte const> message, std::span<std::size_t const> lengths) {
  switch(code_path_of(parameters).register_bits) {
  case 8:
    check_against_byte<Strategy, Preset, std::uint8_t>(parameters, message, lengths);
    break;
  case 16:
    check_against_byte<Strategy, Preset, std::uint16_t>(parameters, message, lengths);
    break;
  case 32:
    check_against_byte<Strategy, Preset, std::uint32_t>(parameters, message, lengths);
    break;
  default:
    check_against_byte<Strategy, Preset, std::uint64_t>(parameters, message, lengths);
  }
}

template <isa Preset> consteval bool sliced_accumulator_round_trip() {
  auto const &engine = crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_sliced, Preset>;
  accumulator<crc_engine<std::uint32_t, crc_lut_sliced, Preset>> running(engine);
  running.update(std::string_view("1234"));
  running.update(std::string_view("56789"));
  return running.value() == std::uint32_t{0xCBF43926};
}

template <isa Preset> void check_constant() {
  static_assert((crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_none, Preset>.compute(std::string_view("123456789"))) == std::uint32_t{0xCBF43926});
  static_assert((crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_nibble, Preset>.compute(std::string_view("123456789"))) ==
                std::uint32_t{0xCBF43926});
  static_assert((crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_byte, Preset>.compute(std::string_view("123456789"))) == std::uint32_t{0xCBF43926});
  static_assert((crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_sliced, Preset>.compute(std::string_view("123456789"))) ==
                std::uint32_t{0xCBF43926});
  static_assert((crc_engine_for<checksum::crc32_iso_hdlc, crc_lut_braided, Preset>.compute(std::string_view("123456789"))) ==
                std::uint32_t{0xCBF43926});
  static_assert((crc_engine_for<checksum::crc16_xmodem, crc_lut_sliced, Preset>.compute(std::string_view("123456789"))) == std::uint16_t{0x31C3});
  static_assert((crc_engine_for<checksum::crc64_xz, crc_lut_sliced, Preset>.compute(std::string_view("123456789"))) ==
                std::uint64_t{0x995DC9BBDF1939FA});
  static_assert(sliced_accumulator_round_trip<Preset>());
}

template <crc_parameters Parameters, class Strategy, isa Preset> void check_common(std::span<std::byte const> buffer) {
  auto const &reference = crc_engine_for<Parameters, crc_lut_byte, isa::generic>;
  auto const &engine = crc_engine_for<Parameters, Strategy, Preset>;
  for(std::size_t length = 0; length <= common_length; ++length) {
    for(std::size_t const offset : offsets) {
      auto const data = buffer.subspan(offset, length);
      ASSERT_EQ((engine.compute(data)), reference.compute(data)) << "length " << length << ", offset " << offset;
    }
  }
}

template <crc_parameters Parameters, isa Preset> void check_common_all(std::span<std::byte const> buffer) {
  check_common<Parameters, crc_lut_none, Preset>(buffer);
  check_common<Parameters, crc_lut_nibble, Preset>(buffer);
  check_common<Parameters, crc_lut_byte, Preset>(buffer);
}

template <class PresetList> struct add_generic_if_missing;

template <> struct add_generic_if_missing<testing::Types<>> {
  using type = testing::Types<isa_test::preset<checksum::isa::generic>>;
};

template <class First, class... Rest> struct add_generic_if_missing<testing::Types<First, Rest...>> {
  using type = std::conditional_t<First::value == checksum::isa::generic, testing::Types<First, Rest...>,
                                  testing::Types<isa_test::preset<checksum::isa::generic>, First, Rest...>>;
};

using all_presets = add_generic_if_missing<isa_test::presets>::type;

template <class Strategy, class PresetList> struct strategy_preset_pairs;

template <class Strategy, class... Presets> struct strategy_preset_pairs<Strategy, testing::Types<Presets...>> {
  using type = testing::Types<std::pair<Strategy, Presets>...>;
};

template <class First, class Second> struct concat_types;

template <class... FirstTypes, class... SecondTypes> struct concat_types<testing::Types<FirstTypes...>, testing::Types<SecondTypes...>> {
  using type = testing::Types<FirstTypes..., SecondTypes...>;
};

using folding_pairs =
    concat_types<strategy_preset_pairs<crc_lut_sliced, all_presets>::type, strategy_preset_pairs<crc_lut_braided, all_presets>::type>::type;

struct pair_name {
  template <class Pair> static std::string GetName(int /*index*/) {
    return std::string(crc_test::strategy_name<typename Pair::first_type>()) + '_' + std::string(isa_test::name(Pair::second_type::value));
  }
};

// Calls check(engine, generic) for CRC-32C on crc_lut_sliced and CRC-64/XZ on crc_lut_braided of Preset against isa::generic.
template <isa Preset, class Check> void for_each_folding_engine(Check check) {
  check(crc_engine_for<checksum::crc32c, crc_lut_sliced, Preset>, crc_engine_for<checksum::crc32c, crc_lut_sliced>);
  check(crc_engine_for<checksum::crc64_xz, crc_lut_braided, Preset>, crc_engine_for<checksum::crc64_xz, crc_lut_braided>);
}

template <class Preset> struct crc_presets : isa_test::preset_test<Preset> {};

TYPED_TEST_SUITE(crc_presets, isa_test::presets, isa_test::preset_name);

// Constant evaluation runs the portable code of every strategy for every preset.
TYPED_TEST(crc_presets, constant_evaluation) {
  constexpr isa preset = TypeParam::value;
  check_constant<preset>();
  auto const buffer = random_bytes(1003, 4);
  EXPECT_EQ((crc_compute<checksum::crc32c, crc_lut_sliced, preset>(at(buffer, 3, 1000))), (crc_compute<checksum::crc32c>(at(buffer, 3, 1000))));
}

// Up to 640 bytes, two steps of the widest loop with every tail; crc_folding_presets covers longer messages.
TYPED_TEST(crc_presets, match_generic_for_every_size) {
  auto const buffer = random_bytes(640 + 8, 1);
  for_each_folding_engine<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_every_size(engine, generic, buffer, 640); });
}

TYPED_TEST(crc_presets, match_generic_for_large_sizes) {
  auto const buffer = random_bytes(1'000'008, 2);
  // CRC-32C alone: the catalog sets of crc_folding_presets run up to 4 KiB.
  engine_checks::check_large_sizes(crc_engine_for<checksum::crc32c, crc_lut_sliced, TypeParam::value>,
                                   crc_engine_for<checksum::crc32c, crc_lut_sliced>, buffer);
}

TYPED_TEST(crc_presets, match_generic_through_updates_and_splits) {
  auto const buffer = random_bytes(3000, 3);
  for_each_folding_engine<TypeParam::value>(
      [&](auto const &engine, auto const &generic) { engine_checks::check_updates_and_splits(engine, generic, buffer); });
}

TYPED_TEST(crc_presets, match_byte_table_for_common_parameters) {
  constexpr isa preset = TypeParam::value;
  auto const buffer = random_bytes(common_length + offsets.back(), 0x5EED);
  std::span<std::byte const> const data(buffer);

  SCOPED_TRACE("crc32_iso_hdlc");
  check_common_all<checksum::crc32_iso_hdlc, preset>(data);

  SCOPED_TRACE("crc16_xmodem");
  check_common_all<checksum::crc16_xmodem, preset>(data);

  SCOPED_TRACE("crc64_xz");
  check_common_all<checksum::crc64_xz, preset>(data);
}

template <class Pair> struct crc_folding_presets : ::testing::Test {
  void SetUp() override { isa_test::skip_unless_cpu_runs(Pair::second_type::value); }
};

TYPED_TEST_SUITE(crc_folding_presets, folding_pairs, pair_name);

TYPED_TEST(crc_folding_presets, match_byte_table_for_every_catalog_set) {
  using Strategy = typename TypeParam::first_type;
  constexpr isa preset = TypeParam::second_type::value;

  auto const message = random_bytes(longest + offsets.back(), 0x5EED);
  auto const all = every_length();
  auto const thresholds = threshold_lengths();
  std::vector<code_path> tested;
  for(crc_test::catalog_check const &set : crc_test::catalog_checks) {
    SCOPED_TRACE(set.name);
    code_path const path = code_path_of(set.parameters);
    bool const first = std::ranges::find(tested, path) == tested.end();
    if(first) {
      tested.push_back(path);
    }
    check_set<Strategy, preset>(set.parameters, message, first ? all : thresholds);
  }
}

} // namespace
