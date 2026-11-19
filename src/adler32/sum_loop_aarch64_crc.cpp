// The Adler-32 checksum loop compiled for isa::aarch64_crc.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_PRESET aarch64_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_ADLER32_ENTRY
#include <checksum_private/fletcher_loops.hpp>
#endif
