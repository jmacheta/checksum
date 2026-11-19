// The MurmurHash3 block loop compiled for isa::aarch64_crc.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_PRESET aarch64_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_CRC
#include <checksum_private/murmur3_loops.hpp>
#endif
