// The MurmurHash3 block loop compiled for isa::x86_vpclmul.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_VPCLMUL
#define CHECKSUM_PRESET x86_vpclmul
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_VPCLMUL
#include <checksum_private/murmur3_loops.hpp>
#endif
