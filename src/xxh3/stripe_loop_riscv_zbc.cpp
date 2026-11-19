// The XXH3 stripe loop compiled for isa::riscv_zbc.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_RISCV_ZBC
#define CHECKSUM_PRESET riscv_zbc
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_ZBC
#include <checksum_private/xxh3_loops.hpp>
#endif
