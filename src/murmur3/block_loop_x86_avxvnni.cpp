// The MurmurHash3 block loop compiled for isa::x86_avxvnni.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_AVXVNNI
#define CHECKSUM_PRESET x86_avxvnni
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVXVNNI
#include <checksum_private/murmur3_loops.hpp>
#endif
