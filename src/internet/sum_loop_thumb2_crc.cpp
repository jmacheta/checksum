// The Internet checksum loop compiled for isa::thumb2_crc.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_THUMB2_CRC
#define CHECKSUM_PRESET thumb2_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_CRC
#include <checksum_private/internet_loops.hpp> // IWYU pragma: keep
#endif
