// CRC engines: reference values, splits, ranges, bit lengths, resume and edge parameters, on every strategy and register.

#include "crc_test_support.hpp"

#include <checksum/byte_range.hpp>
#include <checksum/crc.hpp>
#include <checksum/engine.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <csignal> // IWYU pragma: keep
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace std::literals;
using namespace checksum;
using crc_test::crc_model;
using crc_test::for_each_engine;
using crc_test::models;
using crc_test::tag;

// Arrays of char and char8_t (string literals) are rejected, and so are volatile elements.
static_assert(byte_range<std::string_view>);
static_assert(byte_range<std::array<std::byte, 4> const &>);
static_assert(byte_range<std::vector<unsigned char> &>);
static_assert(byte_range<std::list<char> &>);
static_assert(byte_range<std::span<char>>);
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays): arrays are the case under test.
static_assert(byte_range<std::byte (&)[4]>);
static_assert(byte_range<unsigned char const (&)[4]>);
static_assert(byte_range<signed char (&)[4]>);
static_assert(byte_range<std::u8string_view>);
static_assert(!byte_range<char const (&)[4]>);
static_assert(!byte_range<char (&)[4]>);
static_assert(!byte_range<char8_t const (&)[4]>);
static_assert(!byte_range<std::vector<int> &>);
static_assert(!byte_range<std::vector<std::uint16_t> &>);
static_assert(!byte_range<std::span<std::byte volatile>>);
static_assert(!byte_range<unsigned char volatile (&)[4]>);
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
static_assert(!byte_range<std::span<char const volatile>>);

// Every local model on every strategy at compile time.
template <class Strategy, std::size_t... Index> constexpr bool checks_at_compile_time(std::index_sequence<Index...> /*unused*/) {
  return ((crc_engine_for<crc_test::model_parameters<Index>, Strategy>.compute(crc_test::check_text) == models[Index].check) && ...);
}

template <class Strategy> constexpr bool all_checks = checks_at_compile_time<Strategy>(std::make_index_sequence<models.size()>{});
static_assert(all_checks<crc_lut_none>);
static_assert(all_checks<crc_lut_nibble>);
static_assert(all_checks<crc_lut_byte>);
static_assert(all_checks<crc_lut_sliced>);
static_assert(all_checks<crc_lut_braided>);

inline constexpr auto crc32 = crc_test::model_parameters<9>;     // CRC-32/ISO-HDLC
inline constexpr auto crc15_can = crc_test::model_parameters<6>; // CRC-15/CAN
inline constexpr auto crc12_umts = crc_test::model_parameters<5>;
static_assert(crc32.polynomial.normal_form() == 0x04C11DB7 && crc15_can.polynomial.width() == 15 && crc12_umts.reflect_output);

// Equal inline parameters give the same engine object.
static_assert(crc_engine_for<crc_parameters{.polynomial = 0x83}>.compute("123456789"sv) == 0xF4); // CRC-8/SMBUS
static_assert(crc_test::same_object(&crc_engine_for<crc_test::model_parameters<4>>, &crc_engine_for<crc_parameters{.polynomial = 0x83}>));

static_assert(std::is_same_v<std::remove_cvref_t<decltype(crc_engine_for<crc32>)>, crc_engine<std::uint32_t, crc_lut_none>>);
static_assert(std::is_same_v<crc_engine<std::uint32_t>, crc_engine<std::uint32_t, crc_lut_none>>);
static_assert(std::is_same_v<std::remove_cvref_t<decltype(crc_engine_for<crc15_can, crc_lut_none>)>, crc_engine<std::uint16_t, crc_lut_none>>);

// Byte-range adapters and chunked updates during constant evaluation.
static_assert(crc_engine_for<crc32>.compute("123456789"sv) == 0xCBF43926);
static_assert(crc_engine_for<crc32>.compute(u8"123456789"sv) == 0xCBF43926);
static_assert(crc_engine_for<crc32>.compute(std::array{std::byte{'1'}, std::byte{'2'}, std::byte{'3'}, std::byte{'4'}, std::byte{'5'}, std::byte{'6'},
                                                       std::byte{'7'}, std::byte{'8'}, std::byte{'9'}}) == 0xCBF43926);
static_assert(crc_engine_for<crc32>.compute(std::array<unsigned char, 9>{'1', '2', '3', '4', '5', '6', '7', '8', '9'}) == 0xCBF43926);
constexpr std::string_view long_text = "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.";
static_assert(crc_engine_for<crc32, crc_lut_sliced>.compute(long_text) == crc_engine_for<crc32, crc_lut_none>.compute(long_text));
static_assert(crc_engine_for<crc32, crc_lut_braided>.compute(long_text) == crc_engine_for<crc32, crc_lut_none>.compute(long_text));
static_assert(crc_engine_for<crc32>.update(crc_engine_for<crc32>.update(crc_engine_for<crc32>.initial(), "1234"sv), "56789"sv) ==
              crc_engine_for<crc32>.update(crc_engine_for<crc32>.initial(), "123456789"sv));

constexpr std::array<std::byte, 2> two_bytes{std::byte{0xA5}, std::byte{0x3C}};
static_assert(crc_engine_for<crc15_can>.compute(two_bytes, 16) == crc_engine_for<crc15_can>.compute(two_bytes));
static_assert(crc_engine_for<crc15_can>.update(crc_engine_for<crc15_can>.update(crc_engine_for<crc15_can>.initial(), std::span(two_bytes).first(1)),
                                               std::span(two_bytes).subspan(1),
                                               3) == crc_engine_for<crc15_can>.update(crc_engine_for<crc15_can>.initial(), two_bytes, 11));

TEST(crc, reference_values) {
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      EXPECT_EQ(engine.compute(crc_test::check_text), model.check) << model.name;
      EXPECT_EQ(engine.compute(std::as_bytes(std::span(crc_test::check_text))), model.check) << model.name;
      EXPECT_EQ(engine.finalize(engine.update(engine.initial(), crc_test::check_text)), model.check) << model.name;
    });
  }
}

TEST(crc, initial_and_finalize) {
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      std::uint64_t expected_initial = model.initial_value;
      if(model.reflect_input) {
        expected_initial = 0;
        for(unsigned i = 0; i < model.width; ++i) {
          expected_initial |= ((model.initial_value >> i) & 1U) << (model.width - 1 - i);
        }
      }
      EXPECT_EQ(engine.initial(), expected_initial) << model.name;
      EXPECT_EQ(engine.finalize(engine.initial()), crc_test::reference_crc(model, {})) << model.name;
      EXPECT_LE(engine.initial(), crc_test::mask_of(model.width));
    });
  }
}

// Every range kind gives the same result, including non-contiguous ranges, which are copied in 64-byte chunks.
TEST(crc, byte_ranges) {
  auto const bytes = random_bytes(200, 7);
  std::vector<unsigned char> uchars;
  std::vector<signed char> schars;
  std::list<char> chars;
  std::u8string utf8;
  for(std::byte const byte : bytes) {
    uchars.push_back(std::to_integer<unsigned char>(byte));
    schars.push_back(static_cast<signed char>(std::to_integer<unsigned char>(byte)));
    chars.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
    utf8.push_back(static_cast<char8_t>(std::to_integer<unsigned char>(byte)));
  }
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_crc(model, bytes);
    for_each_engine(model, [&](auto const &engine) {
      EXPECT_EQ(engine.compute(std::span<std::byte const>(bytes)), expected);
      EXPECT_EQ(engine.compute(bytes), expected);
      EXPECT_EQ(engine.compute(uchars), expected);
      EXPECT_EQ(engine.compute(schars), expected);
      EXPECT_EQ(engine.compute(chars), expected);
      EXPECT_EQ(engine.compute(utf8), expected);
      EXPECT_EQ(engine.compute(std::list<char>{}), crc_test::reference_crc(model, {}));
    });
  }
}

// Every length 0..320 (every block and threshold of the loops, at most 256 bytes, with every tail), then every 37th up
// to 1023, at an aligned and a misaligned start, against the reference.
TEST(crc, every_size_and_offset) {
  constexpr std::size_t max_length = 1024;
  constexpr std::size_t every_length = 320;
  constexpr std::size_t length_step = 37;
  auto const message = random_bytes(max_length, 42);
  alignas(64) std::array<std::byte, max_length + 64> buffer{};
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_prefixes(model, message);
    for(std::size_t const offset : {0U, 7U}) {
      std::ranges::copy(message, buffer.begin() + static_cast<std::ptrdiff_t>(offset));
      auto const data = std::span<std::byte const>(buffer).subspan(offset, max_length);
      for_each_engine(model, [&](auto const &engine) {
        for(std::size_t length = 0; length <= max_length; length += length < every_length ? 1 : length_step) {
          auto const state = engine.update(engine.initial(), data.first(length));
          ASSERT_LE(state, crc_test::mask_of(model.width)) << model.name << " length " << length;
          ASSERT_EQ(engine.finalize(state), expected[length]) << model.name << " length " << length << " offset " << offset;
        }
      });
    }
  }
}

// Every byte split of a 64-byte message, every bit split of its first 64 bits, and bit-by-bit updates.
TEST(crc, split_anywhere) {
  auto const message = random_bytes(64, 99);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_crc(model, data);
    auto const expected_64_bits = crc_test::reference_crc(model, data, 64);
    for_each_engine(model, [&](auto const &engine) {
      for(std::size_t split = 0; split <= data.size(); ++split) {
        auto const state = engine.update(engine.update(engine.initial(), data.first(split)), data.subspan(split));
        ASSERT_EQ(engine.finalize(state), expected) << model.name << " split " << split;
      }
      for(std::size_t split = 0; split <= 64; ++split) {
        auto const first = crc_test::extract_bits(data, model.reflect_input, 0, split);
        auto const second = crc_test::extract_bits(data, model.reflect_input, split, 64 - split);
        auto const state = engine.update(engine.update(engine.initial(), first, split), second, 64 - split);
        ASSERT_EQ(engine.finalize(state), expected_64_bits) << model.name << " bit split " << split;
      }
      auto state = engine.initial();
      for(std::size_t bit = 0; bit < 64; ++bit) {
        state = engine.update(state, crc_test::extract_bits(data, model.reflect_input, bit, 1), 1);
      }
      EXPECT_EQ(engine.finalize(state), expected_64_bits) << model.name;
    });
  }
}

// Pieces of every size up to 70 bytes, across the 8-byte words, 16-byte blocks, 40-byte braids and 64-byte steps.
TEST(crc, split_into_chunks) {
  auto const message = random_bytes(300, 97);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_crc(model, data);
    for_each_engine(model, [&](auto const &engine) {
      for(std::size_t chunk_size = 1; chunk_size <= 70; ++chunk_size) {
        ASSERT_EQ(engine_checks::chunked(engine, data, chunk_size), expected) << model.name << " chunk " << chunk_size;
      }
    });
  }
}

// Longer than every block and threshold of the loops, one-shot and in 4099-byte pieces.
TEST(crc, long_message) {
  auto const message = random_bytes(20'000, 96);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_crc(model, data);
    for_each_engine(model, [&](auto const &engine) {
      EXPECT_EQ(engine.compute(data), expected) << model.name;
      EXPECT_EQ(engine_checks::chunked(engine, data, 4099), expected) << model.name;
    });
  }
}

constexpr auto constant_message = random_array<64>();

// Every prefix computed at compile time on every strategy, plus a split into pieces.
TEST(crc, constant_evaluation) {
  static constexpr auto prefixes = [] {
    std::array<std::array<std::uint32_t, constant_message.size() + 1>, 5> result{};
    for(std::size_t size = 0; size <= constant_message.size(); ++size) {
      auto const data = std::span(constant_message).first(size);
      result[0][size] = crc_engine_for<crc32, crc_lut_none>.compute(data);
      result[1][size] = crc_engine_for<crc32, crc_lut_nibble>.compute(data);
      result[2][size] = crc_engine_for<crc32, crc_lut_byte>.compute(data);
      result[3][size] = crc_engine_for<crc32, crc_lut_sliced>.compute(data);
      result[4][size] = crc_engine_for<crc32, crc_lut_braided>.compute(data);
    }
    return result;
  }();
  static constexpr auto split = engine_checks::chunked(crc_engine_for<crc32, crc_lut_sliced>, std::span(constant_message), 7);
  auto const expected = crc_test::reference_prefixes(models[9], constant_message);
  for(auto const &strategy : prefixes) {
    for(std::size_t size = 0; size <= constant_message.size(); ++size) {
      ASSERT_EQ(strategy[size], expected[size]) << "size " << size;
    }
  }
  EXPECT_EQ(split, expected.back());
}

// A stored CRC continues the message after any number of bytes: resume() undoes finalize() for every model, register and strategy.
TEST(crc, resume_from_checksum) {
  auto const message = random_bytes(64, 98);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    auto const expected = crc_test::reference_crc(model, data);
    for_each_engine(model, [&](auto const &engine) {
      static_assert(resumable_engine<std::remove_cvref_t<decltype(engine)>>);
      for(std::size_t split = 0; split <= data.size(); ++split) {
        auto const stored = engine.compute(data.first(split));
        ASSERT_EQ(engine.finalize(engine.resume(stored)), stored) << model.name << " split " << split;
        ASSERT_EQ(engine.finalize(engine.update(engine.resume(stored), data.subspan(split))), expected) << model.name << " split " << split;
      }
    });
  }
}

static_assert(crc_compute<crc32>("123456789"sv) == 0xCBF43926U && crc_compute<crc16_modbus, crc_lut_byte>("123456789"sv) == 0x4B37U);
static_assert(std::same_as<decltype(crc_compute<crc5_usb>("1"sv)), std::uint8_t>);
static_assert([] {
  auto const &engine = crc_engine_for<crc32>;
  return engine.finalize(engine.update(engine.resume(crc_compute<crc32>("1234"sv)), "56789"sv)) == 0xCBF43926U;
}());

// Bit lengths 0..320 against the reference; multiples of 8 equal the byte overloads.
TEST(crc, bit_lengths) {
  auto const message = random_bytes(40, 5);
  std::span<std::byte const> const data(message);
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      for(std::size_t bits = 0; bits <= data.size() * 8; ++bits) {
        auto const result = engine.compute(data, bits);
        ASSERT_EQ(result, crc_test::reference_crc(model, data, bits)) << model.name << " bits " << bits;
        if(bits % 8 == 0) {
          ASSERT_EQ(result, engine.compute(data.first(bits / 8)));
        }
      }
      EXPECT_EQ(engine.compute(data, 0), engine.compute(std::span<std::byte const>{}));
    });
  }
}

// A partial last byte uses only its message bits (bits 0..count-1 if reflected, else 7..8-count).
TEST(crc, tail_bits_use_only_message_bits) {
  for(crc_model const &model : models) {
    for_each_engine(model, [&](auto const &engine) {
      auto const start = engine.update(engine.initial(), crc_test::check_text);
      for(unsigned value = 0; value < 256; ++value) {
        auto const tail = static_cast<std::byte>(value);
        EXPECT_EQ(engine.update(start, std::span(&tail, 1), 0), start);
        for(unsigned count = 1; count < 8; ++count) {
          unsigned const used = model.reflect_input ? (1U << count) - 1 : (0xFFU << (8 - count)) & 0xFFU;
          auto const flipped = static_cast<std::byte>(value ^ (~used & 0xFFU));
          ASSERT_EQ(engine.update(start, std::span(&tail, 1), count), engine.update(start, std::span(&flipped, 1), count)) << model.name;
        }
      }
    });
  }
}

// Edge parameters: random parameters of every width 1..64 in every reflection combination, against the reference. Half of
// the variants use an all-ones initial value and final XOR.
TEST(crc, edge_values) {
  std::mt19937_64 generator(2024);
  auto const message = random_bytes(40, 11);
  std::span<std::byte const> const data(message);
  for(unsigned width = 1; width <= 64; ++width) {
    auto const mask = crc_test::mask_of(width);
    for(unsigned variant = 0; variant < 8; ++variant) {
      bool const all_ones = (variant & 4U) != 0;
      auto const polynomial = (generator() & mask) | 1U;
      crc_model const model{.name = "synthetic",
                            .width = width,
                            .polynomial = polynomial,
                            .initial_value = all_ones ? mask : generator() & mask,
                            .reflect_input = (variant & 1U) != 0,
                            .reflect_output = (variant & 2U) != 0,
                            .final_xor = all_ones ? mask : generator() & mask,
                            .check = 0};
      auto const expected = crc_test::reference_prefixes(model, data);
      for_each_engine(model, [&](auto const &engine) {
        for(std::size_t length = 0; length <= data.size(); ++length) {
          auto const state = engine.update(engine.initial(), data.first(length));
          ASSERT_LE(state, mask) << "width " << width;
          ASSERT_EQ(engine.finalize(state), expected[length]) << "width " << width << " variant " << variant << " length " << length;
        }
        for(std::size_t bits = 1; bits <= 80; ++bits) {
          ASSERT_EQ(engine.compute(data, bits), crc_test::reference_crc(model, data, bits)) << "width " << width << " bits " << bits;
        }
      });
    }
  }
}

// Parameters known only at run time.
TEST(crc, run_time_parameters) {
  std::uint64_t const user_normal_form = 0x04C11DB7;
  unsigned const user_width = 32;
  auto const engine = crc_polynomial::create(user_normal_form, user_width).and_then([](crc_polynomial polynomial) {
    return crc_engine<std::uint64_t>::create(
        {.polynomial = polynomial, .initial_value = 0xFFFFFFFF, .reflect_input = true, .reflect_output = true, .final_xor = 0xFFFFFFFF});
  });
  ASSERT_TRUE(engine);
  EXPECT_EQ(engine->compute("123456789"sv), 0xCBF43926U);
}

// Violated preconditions fail an assert unless NDEBUG is defined.
#ifndef NDEBUG
// The abort of the assert exits without a core dump: a crash reporter that collects it takes about a second.
void exit_on_abort() {
  (void)std::signal(SIGABRT, [](int /*signal*/) { std::_Exit(EXIT_FAILURE); });
}

TEST(crc_DeathTest, preconditions_are_checked) {
  auto const &engine = crc_engine_for<crc15_can>;
  std::array<std::byte, 2> const data{};
  EXPECT_DEATH((exit_on_abort(), (void)engine.update(0x8000, data)), "state <= mask");
  EXPECT_DEATH((exit_on_abort(), (void)engine.finalize(0xFFFF)), "state <= mask");
  EXPECT_DEATH((exit_on_abort(), (void)engine.update(engine.initial(), data, 17)), "in_range");
  EXPECT_DEATH((exit_on_abort(), (void)engine.update(engine.initial(), std::span<std::byte const>{}, 1)), "in_range");
}
#endif

} // namespace
