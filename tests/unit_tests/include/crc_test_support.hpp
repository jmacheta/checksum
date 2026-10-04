#ifndef CHECKSUM_TESTS_CRC_TEST_SUPPORT_HPP
#define CHECKSUM_TESTS_CRC_TEST_SUPPORT_HPP

// Test helpers: CRC models independent of the catalog, a bitwise reference CRC written from the definition, and
// iteration over strategies and register types.

#include <checksum/crc.hpp>

#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace crc_test {

// Polynomial in normal form; initial value and final XOR in natural (unreflected) order.
struct crc_model {
  std::string_view name;
  unsigned width;
  std::uint64_t polynomial;
  std::uint64_t initial_value;
  bool reflect_input;
  bool reflect_output;
  std::uint64_t final_xor;
  std::uint64_t check; // CRC of "123456789"
};

// Published check values, independent of the catalog header.
inline constexpr std::array models{
    crc_model{.name = "CRC-3/GSM",
              .width = 3,
              .polynomial = 0x3,
              .initial_value = 0x0,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x7,
              .check = 0x4},
    crc_model{.name = "CRC-4/G-704",
              .width = 4,
              .polynomial = 0x3,
              .initial_value = 0x0,
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = 0x0,
              .check = 0x7},
    crc_model{.name = "CRC-5/USB",
              .width = 5,
              .polynomial = 0x05,
              .initial_value = 0x1F,
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = 0x1F,
              .check = 0x19},
    crc_model{.name = "CRC-7/MMC",
              .width = 7,
              .polynomial = 0x09,
              .initial_value = 0x00,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x00,
              .check = 0x75},
    crc_model{.name = "CRC-8/SMBUS",
              .width = 8,
              .polynomial = 0x07,
              .initial_value = 0x00,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x00,
              .check = 0xF4},
    crc_model{.name = "CRC-12/UMTS",
              .width = 12,
              .polynomial = 0x80F,
              .initial_value = 0x000,
              .reflect_input = false,
              .reflect_output = true,
              .final_xor = 0x000,
              .check = 0xDAF},
    crc_model{.name = "CRC-15/CAN",
              .width = 15,
              .polynomial = 0x4599,
              .initial_value = 0x0000,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x0000,
              .check = 0x059E},
    crc_model{.name = "CRC-16/XMODEM",
              .width = 16,
              .polynomial = 0x1021,
              .initial_value = 0x0000,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x0000,
              .check = 0x31C3},
    crc_model{.name = "CRC-16/KERMIT",
              .width = 16,
              .polynomial = 0x1021,
              .initial_value = 0x0000,
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = 0x0000,
              .check = 0x2189},
    crc_model{.name = "CRC-32/ISO-HDLC",
              .width = 32,
              .polynomial = 0x04C11DB7,
              .initial_value = 0xFFFFFFFF,
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = 0xFFFFFFFF,
              .check = 0xCBF43926},
    crc_model{.name = "CRC-32/MPEG-2",
              .width = 32,
              .polynomial = 0x04C11DB7,
              .initial_value = 0xFFFFFFFF,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x00000000,
              .check = 0x0376E6E7},
    crc_model{.name = "CRC-64/ECMA-182",
              .width = 64,
              .polynomial = 0x42F0E1EBA9EA3693,
              .initial_value = 0x0,
              .reflect_input = false,
              .reflect_output = false,
              .final_xor = 0x0,
              .check = 0x6C40DF5F0B497347},
    crc_model{.name = "CRC-64/XZ",
              .width = 64,
              .polynomial = 0x42F0E1EBA9EA3693,
              .initial_value = ~std::uint64_t{0},
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = ~std::uint64_t{0},
              .check = 0x995DC9BBDF1939FA},
    crc_model{.name = "CRC-64/GO-ISO",
              .width = 64,
              .polynomial = 0x1B,
              .initial_value = ~std::uint64_t{0},
              .reflect_input = true,
              .reflect_output = true,
              .final_xor = ~std::uint64_t{0},
              .check = 0xB90956C775A41001},
};

constexpr checksum::crc_parameters to_parameters(crc_model const &model);

// models[Index] as library parameters, usable as a template argument.
template <std::size_t Index> inline constexpr checksum::crc_parameters model_parameters = to_parameters(models[Index]);

// Compares addresses without the self-comparison warning of a literal `&a == &b`.
constexpr bool same_object(void const *first, void const *second);

inline constexpr std::string_view check_text = "123456789";

constexpr std::uint64_t mask_of(unsigned width);

// One message bit at a time into a most-significant-bit-first register; reflection applies to input bits and the result.
class bitwise_reference {
public:
  explicit bitwise_reference(crc_model const &model);

  void feed_bit(unsigned bit);

  // Feeds count bits (1..8) of byte in message order: least significant bit first if the input is reflected.
  void feed(std::byte byte, unsigned count = 8);

  [[nodiscard]] std::uint64_t value() const;

private:
  crc_model model;
  std::uint64_t remainder; // most significant bit first
};

// bit_length is at most 8 * data.size().
inline std::uint64_t reference_crc(crc_model const &model, std::span<std::byte const> data, std::size_t bit_length);

inline std::uint64_t reference_crc(crc_model const &model, std::span<std::byte const> data);

// CRCs of every prefix data[0, n), n = 0..data.size().
inline std::vector<std::uint64_t> reference_prefixes(crc_model const &model, std::span<std::byte const> data);

// Packs message bits [first, first + count) of data into bytes in message bit order, for bit-split tests.
inline std::vector<std::byte> extract_bits(std::span<std::byte const> data, bool reflect_input, std::size_t first, std::size_t count);

template <class Type> using tag = std::type_identity<Type>;

template <class Visitor> void for_each_strategy(Visitor visit);

// Calls visit(tag<Register>{}) for every register type with at least width bits.
template <class Visitor> void for_each_register(unsigned width, Visitor visit);

// Calls visit(engine) for every built-in strategy and every register type that fits the model.
template <class Visitor> void for_each_engine(crc_model const &model, Visitor visit);

constexpr checksum::crc_parameters to_parameters(crc_model const &model) {
  return {.polynomial = *checksum::crc_polynomial::create(model.polynomial, model.width),
          .initial_value = model.initial_value,
          .reflect_input = model.reflect_input,
          .reflect_output = model.reflect_output,
          .final_xor = model.final_xor};
}

constexpr bool same_object(void const *first, void const *second) { return first == second; }

constexpr std::uint64_t mask_of(unsigned width) { return width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1; }

inline bitwise_reference::bitwise_reference(crc_model const &model) : model(model), remainder(model.initial_value) {}

inline void bitwise_reference::feed_bit(unsigned bit) {
  bool const feedback = (((remainder >> (model.width - 1)) & 1U) ^ bit) != 0;
  remainder = (remainder << 1U) & mask_of(model.width);
  if(feedback) {
    remainder ^= model.polynomial;
  }
}

inline void bitwise_reference::feed(std::byte byte, unsigned count) {
  auto const value = std::to_integer<unsigned>(byte);
  for(unsigned index = 0; index < count; ++index) {
    feed_bit(model.reflect_input ? (value >> index) & 1U : (value >> (7 - index)) & 1U);
  }
}

inline std::uint64_t bitwise_reference::value() const {
  std::uint64_t result = remainder;
  if(model.reflect_output) {
    result = 0;
    for(unsigned index = 0; index < model.width; ++index) {
      result |= ((remainder >> index) & 1U) << (model.width - 1 - index);
    }
  }
  return result ^ model.final_xor;
}

inline std::uint64_t reference_crc(crc_model const &model, std::span<std::byte const> data, std::size_t bit_length) {
  bitwise_reference reference(model);
  for(std::size_t index = 0; index < bit_length / 8; ++index) {
    reference.feed(data[index]);
  }
  if(bit_length % 8 != 0) {
    reference.feed(data[bit_length / 8], static_cast<unsigned>(bit_length % 8));
  }
  return reference.value();
}

inline std::uint64_t reference_crc(crc_model const &model, std::span<std::byte const> data) { return reference_crc(model, data, data.size() * 8); }

inline std::vector<std::uint64_t> reference_prefixes(crc_model const &model, std::span<std::byte const> data) {
  std::vector<std::uint64_t> result;
  result.reserve(data.size() + 1);
  bitwise_reference reference(model);
  result.push_back(reference.value());
  for(std::byte const byte : data) {
    reference.feed(byte);
    result.push_back(reference.value());
  }
  return result;
}

inline std::vector<std::byte> extract_bits(std::span<std::byte const> data, bool reflect_input, std::size_t first, std::size_t count) {
  std::vector<std::byte> result((count + 7) / 8);
  for(std::size_t index = 0; index < count; ++index) {
    std::size_t const source = first + index;
    auto const value = std::to_integer<unsigned>(data[source / 8]);
    unsigned const bit = reflect_input ? (value >> (source % 8)) & 1U : (value >> (7 - (source % 8))) & 1U;
    unsigned const position = reflect_input ? static_cast<unsigned>(index % 8) : static_cast<unsigned>(7 - (index % 8));
    result[index / 8] |= static_cast<std::byte>(bit << position);
  }
  return result;
}

template <class Visitor> void for_each_strategy(Visitor visit) {
  visit(tag<checksum::crc_lut_none>{});
  visit(tag<checksum::crc_lut_nibble>{});
  visit(tag<checksum::crc_lut_byte>{});
  visit(tag<checksum::crc_lut_sliced>{});
  visit(tag<checksum::crc_lut_braided>{});
}

template <class Visitor> void for_each_register(unsigned width, Visitor visit) {
  if(width <= 8) {
    visit(tag<std::uint8_t>{});
  }
  if(width <= 16) {
    visit(tag<std::uint16_t>{});
  }
  if(width <= 32) {
    visit(tag<std::uint32_t>{});
  }
  visit(tag<std::uint64_t>{});
}

template <class Visitor> void for_each_engine(crc_model const &model, Visitor visit) {
  auto const parameters = to_parameters(model);
  for_each_strategy([&]<class Strategy>(tag<Strategy>) {
    for_each_register(model.width, [&]<class Register>(tag<Register>) {
      auto const engine = checksum::crc_engine<Register, Strategy>::create(parameters);
      if(!engine) {
        ADD_FAILURE() << "crc_engine::create failed for " << model.name;
        return;
      }
      visit(*engine);
    });
  });
}

} // namespace crc_test

#endif // CHECKSUM_TESTS_CRC_TEST_SUPPORT_HPP
