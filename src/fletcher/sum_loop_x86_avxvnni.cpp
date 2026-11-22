// The Fletcher checksum loop compiled for isa::x86_avxvnni.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_AVXVNNI
#define CHECKSUM_PRESET x86_avxvnni
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVXVNNI
#define CHECKSUM_FLETCHER_ENTRY
#include <checksum_private/fletcher_loops.hpp> // IWYU pragma: keep
#endif
