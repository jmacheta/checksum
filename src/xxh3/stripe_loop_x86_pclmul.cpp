// The XXH3 stripe loop compiled for isa::x86_pclmul.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_PCLMUL
#define CHECKSUM_PRESET x86_pclmul
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_PCLMUL
#include <checksum_private/xxh3_loops.hpp>
#endif
