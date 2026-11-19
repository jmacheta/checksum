// The MurmurHash3 block loop compiled for isa::x86_pclmul.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_PCLMUL
#define CHECKSUM_PRESET x86_pclmul
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_PCLMUL
#include <checksum_private/murmur3_loops.hpp>
#endif
