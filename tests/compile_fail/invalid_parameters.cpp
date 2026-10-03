// crc_engine_for<P> does not compile for invalid parameters, here an initial value wider than the CRC.
#include <checksum/crc.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
constexpr std::uint64_t initial_value = 0x1FFFF; // wider than 16 bits
#else
constexpr std::uint64_t initial_value = 0xFFFF;
#endif

constexpr checksum::crc_parameters parameters{.polynomial = *checksum::crc_polynomial::create(0x1021, 16), .initial_value = initial_value};

[[maybe_unused]] auto const result = checksum::crc_engine_for<parameters>.compute(std::span<std::byte const>{});
