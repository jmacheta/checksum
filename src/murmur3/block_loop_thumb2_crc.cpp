// The MurmurHash3 block loop compiled for isa::thumb2_crc.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_THUMB2_CRC
#define CHECKSUM_PRESET thumb2_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_CRC
#include <checksum_private/murmur3_loops.hpp>
#endif
