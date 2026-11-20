// The XXH3 stripe loop compiled for isa::riscv_v.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_RISCV_V
#define CHECKSUM_PRESET riscv_v
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_V
#include <checksum_private/xxh3_loops.hpp>
#endif
