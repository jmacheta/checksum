// The Internet checksum loop compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#include <checksum_private/internet_loops.hpp> // IWYU pragma: keep
#endif
