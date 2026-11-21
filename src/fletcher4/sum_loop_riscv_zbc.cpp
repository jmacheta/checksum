// The fletcher4 checksum loop compiled for isa::riscv_zbc.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_RISCV_ZBC
#define CHECKSUM_PRESET riscv_zbc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_ZBC
#include <checksum_private/fletcher4_loops.hpp> // IWYU pragma: keep
#endif
