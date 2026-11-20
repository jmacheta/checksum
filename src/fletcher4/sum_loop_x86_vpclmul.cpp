// The fletcher4 checksum loop compiled for isa::x86_vpclmul.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_VPCLMUL
#define CHECKSUM_PRESET x86_vpclmul
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_VPCLMUL
#include <checksum_private/fletcher4_loops.hpp> // IWYU pragma: keep
#endif
