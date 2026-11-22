// The XXH3 stripe loop compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#include <checksum_private/xxh3_loops.hpp> // IWYU pragma: keep
#endif
