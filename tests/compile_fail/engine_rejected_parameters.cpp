// crc_engine_for<P, S> is ill-formed when S::make_table rejects P. S is a constexpr strategy that accepts only the
// CRC-16/CCITT polynomial.
#include <checksum/crc.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Behaves like crc_lut_none for the CRC-16/CCITT polynomial and rejects any other.
struct ccitt_only {
  template <class Register> using table_type = checksum::crc_lut_none::table_type<Register>;

  template <class Register> static constexpr std::optional<table_type<Register>> make_table(checksum::crc_parameters const &parameters) noexcept;

  template <class Register>
  static constexpr Register update(table_type<Register> const &table, Register state, std::span<std::byte const> data) noexcept;
};

template <class Register>
constexpr std::optional<ccitt_only::table_type<Register>> ccitt_only::make_table(checksum::crc_parameters const &parameters) noexcept {
  if(parameters.polynomial != *checksum::crc_polynomial::create(0x1021, 16)) {
    return std::nullopt;
  }
  return checksum::crc_lut_none::make_table<Register>(parameters);
}

template <class Register>
constexpr Register ccitt_only::update(table_type<Register> const &table, Register state, std::span<std::byte const> data) noexcept {
  return checksum::crc_lut_none::update<Register>(table, state, data);
}

static_assert(checksum::crc_strategy_for<ccitt_only, std::uint16_t>);

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
constexpr std::uint64_t normal_form = 0x8005;
#else
constexpr std::uint64_t normal_form = 0x1021;
#endif

constexpr checksum::crc_parameters parameters{.polynomial = *checksum::crc_polynomial::create(normal_form, 16)};

[[maybe_unused]] auto const result = checksum::crc_engine_for<parameters, ccitt_only>.compute(std::span<std::byte const>{});
