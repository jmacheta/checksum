// A crc_accumulator cannot be created from a temporary engine: it stores the engine's address, which would dangle.
#include <checksum/crc.hpp>

#include <cstdint>

constexpr checksum::crc_parameters parameters{.polynomial = *checksum::crc_polynomial::create(0x1021, 16)};

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
auto accumulator = checksum::crc_accumulator{*checksum::crc_engine<std::uint16_t>::create(parameters)};
#else
auto const engine = *checksum::crc_engine<std::uint16_t>::create(parameters);
auto accumulator = checksum::crc_accumulator{engine};
#endif
