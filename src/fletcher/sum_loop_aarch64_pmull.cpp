// The Fletcher checksum loop compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_FLETCHER_ENTRY
#include <checksum_private/fletcher_loops.hpp> // IWYU pragma: keep
#endif
