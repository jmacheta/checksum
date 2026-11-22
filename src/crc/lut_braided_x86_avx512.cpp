// The crc_lut_braided loops compiled for isa::x86_avx512.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_AVX512
#define CHECKSUM_PRESET x86_avx512
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_AVX512
#define CHECKSUM_CRC_BRAIDED_ENTRY
#include <checksum_private/crc_table_loops.hpp> // IWYU pragma: keep
#endif
