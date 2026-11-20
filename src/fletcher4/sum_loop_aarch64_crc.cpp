// The fletcher4 checksum loop compiled for isa::aarch64_crc.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_PRESET aarch64_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_CRC
#include <checksum_private/fletcher4_loops.hpp> // IWYU pragma: keep
#endif
