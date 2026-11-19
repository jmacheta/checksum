// A precondition assert that fails during constant evaluation stops compilation. NDEBUG is undefined so the
// assert also fires in Release configurations.
#undef NDEBUG

#include <checksum/crc.hpp>

#include <array>
#include <cstddef>

constexpr checksum::crc_parameters test_parameters{.polynomial = *checksum::crc_polynomial::create(0x1021, 16)};

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
constexpr std::size_t test_bit_length = 9; // more bits than the one byte holds
#else
constexpr std::size_t test_bit_length = 8;
#endif

constexpr std::array<std::byte, 1> test_data{std::byte{0x5A}};
[[maybe_unused]] constexpr auto test_state =
    checksum::crc_engine_for<test_parameters>.update(checksum::crc_engine_for<test_parameters>.initial(), test_data, test_bit_length);
