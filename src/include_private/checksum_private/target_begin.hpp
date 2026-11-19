// No include guard: a preset source includes this header once, after its std and public headers and before the private
// headers it compiles for the preset, which CHECKSUM_PRESET names (an isa value). If CHECKSUM_PRESET_TARGET is defined, its
// target options from presets.hpp apply to every function defined up to target_end.hpp.

#include <checksum/isa.hpp>

static_assert(checksum::isa_available(checksum::isa::CHECKSUM_PRESET), "presets.hpp builds a preset that isa_available() rejects");

#define CHECKSUM_PRAGMA(text) CHECKSUM_PRAGMA_TEXT(text)
#define CHECKSUM_PRAGMA_TEXT(text) _Pragma(#text)

// GCC 15 has no target pragma on RISC-V: there the kernels carry their target attribute themselves. MSVC needs none.
#if defined(CHECKSUM_PRESET_TARGET) && defined(__GNUC__) && !defined(__riscv)
#if defined(__clang__)
// The intrinsics carry their own target attributes, which Clang 18 lets the pushed one override for some but not others,
// then refuses to inline one into another: include them before the push.
#if defined(__aarch64__)
#include <arm_acle.h>
#include <arm_neon.h>
#elif defined(__x86_64__)
#include <immintrin.h>
#endif
CHECKSUM_PRAGMA(clang attribute push(__attribute__((target(CHECKSUM_PRESET_TARGET))), apply_to = function))
#else
#pragma GCC push_options
CHECKSUM_PRAGMA(GCC target(CHECKSUM_PRESET_TARGET))
#endif
#endif
