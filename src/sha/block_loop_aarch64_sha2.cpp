// The SHA-1 and SHA-2 block loops compiled for isa::aarch64_sha2.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_AARCH64_SHA2
#define CHECKSUM_PRESET aarch64_sha2
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_AARCH64_SHA2
#include <checksum_private/sha_loops.hpp> // IWYU pragma: keep
#endif
