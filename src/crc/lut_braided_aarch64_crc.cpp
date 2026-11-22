// The crc_lut_braided loops compiled for isa::aarch64_crc.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_PRESET aarch64_crc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_CRC
#define CHECKSUM_CRC_BRAIDED_ENTRY
#include <checksum_private/crc_table_loops.hpp> // IWYU pragma: keep
#endif
