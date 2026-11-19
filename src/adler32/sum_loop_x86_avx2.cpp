// The Adler-32 checksum loop compiled for isa::x86_avx2.

#include <checksum_private/presets.hpp>

#ifdef CHECKSUM_TARGET_X86_AVX2
#define CHECKSUM_PRESET x86_avx2
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVX2
#define CHECKSUM_ADLER32_ENTRY
#include <checksum_private/fletcher_loops.hpp>
#endif
