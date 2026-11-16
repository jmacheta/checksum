// No include guard: a preset source includes this header once, after its std and public headers and before the private
// headers it compiles for the preset, which CHECKSUM_PRESET names (an isa value). If CHECKSUM_PRESET_TARGET is defined, its
// target options from presets.hpp apply to every function defined up to target_end.hpp.

#include <checksum/isa.hpp>

static_assert(checksum::isa_available(checksum::isa::CHECKSUM_PRESET), "presets.hpp builds a preset that isa_available() rejects");

#define CHECKSUM_PRAGMA(text) CHECKSUM_PRAGMA_TEXT(text)
#define CHECKSUM_PRAGMA_TEXT(text) _Pragma(#text)

// GCC 15 has no target pragma on RISC-V: there the kernels carry their target attribute themselves.
#if defined(CHECKSUM_PRESET_TARGET) && !defined(__riscv)
#if defined(__clang__)
CHECKSUM_PRAGMA(clang attribute push(__attribute__((target(CHECKSUM_PRESET_TARGET))), apply_to = function))
#else
#pragma GCC push_options
CHECKSUM_PRAGMA(GCC target(CHECKSUM_PRESET_TARGET))
#endif
#endif
