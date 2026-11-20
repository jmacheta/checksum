// The crc_lut_braided loops compiled for isa::aarch64_pmull.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_PRESET aarch64_pmull
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_PMULL
#define CHECKSUM_CRC_BRAIDED_ENTRY
#include <checksum_private/crc_table_loops.hpp> // IWYU pragma: keep
#endif
