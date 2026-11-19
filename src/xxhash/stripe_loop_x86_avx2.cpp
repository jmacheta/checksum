// The xxHash stripe loop compiled for isa::x86_avx2.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_AVX2
#define CHECKSUM_PRESET x86_avx2
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVX2
#include <checksum_private/xxhash_loops.hpp>
#endif
