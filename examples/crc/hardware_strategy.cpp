// A user-defined strategy driving the CRC unit of STM32 F0/F3/F7/G0/G4/H7/L4 parts. The library prepares and
// finalizes the CRC; the strategy only feeds whole bytes.
//
// A software model stands in for the peripheral so the example runs on any host. On the device,
// stm32_crc_registers is the volatile register block at the peripheral's address.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <span>

using namespace checksum;

// Model of the CR, INIT, POL and DR registers. The unit shifts most significant bit first; REV_IN reverses each
// written byte and REV_OUT reverses the result over the polynomial size.
class stm32_crc_registers {
public:
  // Writes POL (normal form), CR.POLYSIZE (7, 8, 16 or 32), REV_IN/REV_OUT and INIT, then CR.RESET loads INIT.
  // initial is in the unit's most-significant-bit-first order.
  void configure(std::uint32_t polynomial, unsigned size, bool reverse, std::uint32_t initial) noexcept;

  // An 8-bit write to DR.
  void write(std::byte value) noexcept;

  [[nodiscard]] std::uint32_t read() const noexcept;

private:
  std::uint32_t pol = 0;
  std::uint32_t mask = 0; // POLYSIZE low bits set
  unsigned polysize = 0;
  bool reflected = false; // REV_IN and REV_OUT
  std::uint32_t crc = 0;
};

inline stm32_crc_registers crc_peripheral;

// A strategy provides table_type, make_table and update. The state passes in and out of each update, so one unit
// serves any number of engines, one computation at a time; not constexpr, so engines come from crc_engine::create.
struct stm32_crc_unit {
  // make_table's result: the parameters as register values instead of a lookup table.
  struct configuration {
    std::uint32_t polynomial = 0;
    std::uint8_t size = 0;
    bool reverse = false; // reflected input
  };

  template <class Register> using table_type = configuration;

  // Returns std::nullopt for a register other than std::uint32_t or a width the unit lacks, so create() fails.
  template <class Register> static std::optional<configuration> make_table(crc_parameters const &parameters) noexcept;

  // state is the CRC register, bit-reversed when the input is reflected.
  template <class Register> static Register update(configuration const &table, Register state, std::span<std::byte const> data) noexcept;
};

static_assert(crc_strategy_for<stm32_crc_unit, std::uint32_t>);

// Reverses the low size bits of value; size is 1..32.
constexpr std::uint32_t reverse_bits(std::uint32_t value, unsigned size) noexcept {
  std::uint32_t result = 0;
  for(unsigned bit = 0; bit < size; ++bit) {
    result = (result << 1U) | ((value >> bit) & 1U);
  }
  return result;
}

void stm32_crc_registers::configure(std::uint32_t polynomial, unsigned size, bool reverse, std::uint32_t initial) noexcept {
  pol = polynomial;
  polysize = size;
  mask = size == 32 ? ~std::uint32_t{0} : (std::uint32_t{1} << size) - 1U;
  reflected = reverse;
  crc = initial & mask;
}

void stm32_crc_registers::write(std::byte value) noexcept {
  auto byte = std::to_integer<std::uint32_t>(value);
  if(reflected) {
    byte = reverse_bits(byte, 8);
  }
  for(unsigned bit = 8; bit-- > 0;) {
    bool const feedback = (((crc >> (polysize - 1U)) ^ (byte >> bit)) & 1U) != 0;
    crc = ((crc << 1U) ^ (feedback ? pol : 0U)) & mask;
  }
}

std::uint32_t stm32_crc_registers::read() const noexcept { return reflected ? reverse_bits(crc, polysize) : crc; }

template <class Register> std::optional<stm32_crc_unit::configuration> stm32_crc_unit::make_table(crc_parameters const &parameters) noexcept {
  unsigned const width = parameters.polynomial.width();
  if(!std::same_as<Register, std::uint32_t> || (width != 7 && width != 8 && width != 16 && width != 32)) {
    return std::nullopt;
  }
  return configuration{.polynomial = static_cast<std::uint32_t>(parameters.polynomial.normal_form()),
                       .size = static_cast<std::uint8_t>(width),
                       .reverse = parameters.reflect_input};
}

template <class Register> Register stm32_crc_unit::update(configuration const &table, Register state, std::span<std::byte const> data) noexcept {
  // A reflected state is the unit's register bit-reversed: reverse it for INIT, and REV_OUT reverses the result back.
  crc_peripheral.configure(table.polynomial, table.size, table.reverse, table.reverse ? reverse_bits(state, table.size) : state);
  for(std::byte const value : data) {
    crc_peripheral.write(value);
  }
  return static_cast<Register>(crc_peripheral.read());
}

int main() {
  std::array<std::byte, 9> const message{std::byte{'1'}, std::byte{'2'}, std::byte{'3'}, std::byte{'4'}, std::byte{'5'},
                                         std::byte{'6'}, std::byte{'7'}, std::byte{'8'}, std::byte{'9'}};
  // Each supported parameter set must match the library's default strategy.
  bool all_match = true;
  auto const check = [&](char const *name, crc_parameters const &parameters, std::uint32_t expected) {
    auto const engine = crc_engine<std::uint32_t, stm32_crc_unit>::create(parameters);
    if(!engine) {
      std::println("{:16} rejected by make_table", name);
      all_match = false;
      return;
    }
    std::uint32_t const crc = engine->compute(message);
    std::println("{:16} 0x{:08X} ({})", name, crc, crc == expected ? "matches crc_lut_none" : "MISMATCH");
    all_match = all_match && crc == expected;
  };
  check("CRC-32/MPEG-2", crc32_mpeg_2, crc_engine_for<crc32_mpeg_2>.compute(message));
  check("CRC-32/ISO-HDLC", crc32_iso_hdlc, crc_engine_for<crc32_iso_hdlc>.compute(message));
  check("CRC-16/MODBUS", crc16_modbus, crc_engine_for<crc16_modbus>.compute(message));
  check("CRC-16/XMODEM", crc16_xmodem, crc_engine_for<crc16_xmodem>.compute(message));
  check("CRC-8/SMBUS", crc8_smbus, crc_engine_for<crc8_smbus>.compute(message));
  check("CRC-7/MMC", crc7_mmc, crc_engine_for<crc7_mmc>.compute(message));
  // A width the unit cannot compute is rejected when the engine is created.
  std::println("CRC-12/UMTS      {}", crc_engine<std::uint32_t, stm32_crc_unit>::create(crc12_umts) ? "accepted" : "rejected by make_table");
  return all_match ? 0 : 1;
}
