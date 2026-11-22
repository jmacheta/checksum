// The fletcher4 checksum loop compiled for isa::thumb2_neon.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_THUMB2_NEON
#define CHECKSUM_PRESET thumb2_neon
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_NEON
#include <checksum_private/fletcher4_loops.hpp> // IWYU pragma: keep
#endif
