// The MurmurHash3 block loop compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#include <checksum_private/murmur3_loops.hpp>
#endif
