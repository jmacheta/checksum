// The SHA-1 and SHA-2 block loops compiled for isa::x86_sha.

#include <checksum_private/presets.hpp> // IWYU pragma: keep

#ifdef CHECKSUM_TARGET_X86_SHA
#define CHECKSUM_PRESET x86_sha
#define CHECKSUM_PRESET_TARGET CHECKSUM_TARGET_X86_SHA
#include <checksum_private/sha_loops.hpp> // IWYU pragma: keep
#endif
