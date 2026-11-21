// The crc_lut_sliced loops compiled for isa::riscv_zbc.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_RISCV_ZBC
#define CHECKSUM_PRESET riscv_zbc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_ZBC
#define CHECKSUM_CRC_SLICED_ENTRY
#include <checksum_private/crc_table_loops.hpp> // IWYU pragma: keep
#endif
