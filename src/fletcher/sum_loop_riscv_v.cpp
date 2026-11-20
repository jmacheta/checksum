// The Fletcher checksum loop compiled for isa::riscv_v.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_RISCV_V
#define CHECKSUM_PRESET riscv_v
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_RISCV_V
#define CHECKSUM_FLETCHER_ENTRY
#include <checksum_private/fletcher_loops.hpp> // IWYU pragma: keep
#endif
