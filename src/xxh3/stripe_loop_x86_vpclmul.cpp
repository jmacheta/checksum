// The XXH3 stripe loop compiled for isa::x86_vpclmul.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_VPCLMUL
#define CHECKSUM_PRESET x86_vpclmul
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_VPCLMUL
#include <checksum_private/xxh3_loops.hpp> // IWYU pragma: keep
#endif
