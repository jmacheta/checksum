// No include guard: ends the target region of target_begin.hpp.

#if defined(CHECKSUM_PRESET_TARGET) && !defined(__riscv)
#if defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif
#endif
