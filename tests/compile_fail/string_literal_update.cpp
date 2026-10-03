// byte_range rejects string literals, since their terminating '\0' would be hashed.
#include <checksum/crc.hpp>

#include <cstdint>
#include <string_view>

using namespace std::literals;

constexpr checksum::crc_parameters parameters{.polynomial = *checksum::crc_polynomial::create(0x1021, 16)};
auto const engine = *checksum::crc_engine<std::uint16_t>::create(parameters);

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
auto const state = engine.update(engine.initial(), "123456789");
#else
auto const state = engine.update(engine.initial(), "123456789"sv);
#endif
