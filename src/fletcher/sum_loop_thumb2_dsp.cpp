// The Fletcher checksum loop compiled for isa::thumb2_dsp.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_THUMB2_DSP
#define CHECKSUM_PRESET thumb2_dsp
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_THUMB2_DSP
#define CHECKSUM_FLETCHER_ENTRY
#include <checksum_private/fletcher_loops.hpp> // IWYU pragma: keep
#endif
