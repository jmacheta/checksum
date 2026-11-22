// The crc_lut_braided loops for isa::generic, compiled with the build's own flags: braiding, no kernel.

#define CHECKSUM_PRESET generic
#define CHECKSUM_CRC_BRAIDED_ENTRY
#include <checksum_private/crc_table_loops.hpp> // IWYU pragma: keep
