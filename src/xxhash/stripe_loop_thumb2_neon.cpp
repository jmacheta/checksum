// The xxHash stripe loop compiled for isa::thumb2_neon.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_THUMB2_NEON
#define CHECKSUM_PRESET thumb2_neon
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_NEON
#include <checksum_private/xxhash_loops.hpp>
#endif
