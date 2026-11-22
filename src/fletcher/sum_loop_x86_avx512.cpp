// The Fletcher checksum loop compiled for isa::x86_avx512.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_AVX512
#define CHECKSUM_PRESET x86_avx512
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVX512
#define CHECKSUM_FLETCHER_ENTRY
#include <checksum_private/fletcher_loops.hpp> // IWYU pragma: keep
#endif
