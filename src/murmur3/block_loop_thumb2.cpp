// The MurmurHash3 block loop compiled for isa::thumb2.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_THUMB2
#define CHECKSUM_PRESET thumb2
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2
#include <checksum_private/murmur3_loops.hpp>
#endif
