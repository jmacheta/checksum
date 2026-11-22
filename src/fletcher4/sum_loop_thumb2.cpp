// The fletcher4 checksum loop compiled for isa::thumb2.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_THUMB2
#define CHECKSUM_PRESET thumb2
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2
#include <checksum_private/fletcher4_loops.hpp> // IWYU pragma: keep
#endif
