// The crc_lut_sliced loops for isa::generic, compiled with the build's own flags: slicing-by-8, no kernel.

#define CHECKSUM_PRESET generic
#define CHECKSUM_CRC_SLICED_ENTRY
#include <checksum_private/crc_table_loops.hpp>
