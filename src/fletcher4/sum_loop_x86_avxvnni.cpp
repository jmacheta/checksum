// The fletcher4 checksum loop compiled for isa::x86_avxvnni.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_AVXVNNI
#define CHECKSUM_PRESET x86_avxvnni
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVXVNNI
#include <checksum_private/fletcher4_loops.hpp>
#endif
