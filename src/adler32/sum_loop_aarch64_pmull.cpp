// The Adler-32 checksum loop compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_ADLER32_ENTRY
#include <checksum_private/fletcher_loops.hpp>
#endif
