// The Adler-32 checksum loop compiled for isa::thumb2_neon.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_THUMB2_NEON
#define CHECKSUM_PRESET thumb2_neon
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_NEON
#define CHECKSUM_PRESET_ARM_NEON
#define CHECKSUM_ADLER32_ENTRY
#include <checksum_private/fletcher_loops.hpp>
#endif
