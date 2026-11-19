// The crc_lut_sliced loops compiled for isa::riscv_v.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_RISCV_V
#define CHECKSUM_PRESET riscv_v
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_V
#define CHECKSUM_CRC_SLICED_ENTRY
#include <checksum_private/crc_table_loops.hpp>
#endif
