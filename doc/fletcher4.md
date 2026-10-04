# fletcher4 user guide

`checksum` computes fletcher4, the checksum of the ZFS file system, at compile time or at run time. Everything is in
one header, `checksum/fletcher4.hpp`. The Fletcher-16, Fletcher-32 and Fletcher-64 checksums, which use a modulus and
two sums, have their own guide: [fletcher.md](fletcher.md).

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. It has no tables,
allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/fletcher4.hpp>
using namespace std::literals;

// One call.
checksum::fletcher4_value value = checksum::fletcher4_compute(std::span(block));

// Incremental: a message may be split anywhere.
checksum::fletcher4_state state;
state = checksum::fletcher4_update(state, first_part);
state = checksum::fletcher4_update(state, second_part);
checksum::fletcher4_value checksum_words = checksum::fletcher4_finalize(state);

// Compile time.
static_assert(checksum::fletcher4_compute("abcdefgh"sv) ==
              checksum::fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
```

`examples/fletcher4` has complete programs: the checksum of a constant block computed at compile time, the checksum of
a file read in chunks, and verifying a 4 KiB block against its stored checksum as ZFS does.

## 2. Definition

fletcher4 reads the message as 32-bit little-endian words and keeps four 64-bit sums, all starting at 0. For each word
`sum1` adds the word, `sum2` adds `sum1`, `sum3` adds `sum2` and `sum4` adds `sum3`, all modulo 2^64. The checksum is
`{sum1, sum2, sum3, sum4}`, the order of the four words of the ZFS `zio_cksum_t`.

Words are little-endian on every host. On a little-endian host the result equals `fletcher_4_native` of OpenZFS, on a
big-endian host `fletcher_4_byteswap`. The test values were computed with `fletcher_4_scalar_native` of OpenZFS on a
little-endian host.

ZFS checksums whole words only. Here a message may end inside a word: the last word is padded with zero bytes, so
`"abc"` gives the same checksum as `"abc\0"`. During streaming the bytes of an unfinished word are already added to
`sum1`; `fletcher4_finalize` completes the padded word by adding to `sum2`, `sum3` and `sum4`.

## 3. API

| Name | What it does |
| --- | --- |
| `fletcher4_state` | `sum1` to `sum4` and `word_offset`, the bytes of an unfinished word already in `sum1`. A plain value: copy it, compare it, store it. The default is the empty message. |
| `fletcher4_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `fletcher4_finalize(state)` | The checksum, an unfinished word padded with zero bytes. |
| `fletcher4_compute(data)` | `fletcher4_finalize(fletcher4_update(fletcher4_state{}, data))`. |
| `fletcher4_value` | The checksum: `std::array<std::uint64_t, 4>` holding `sum1` to `sum4`. |

All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. Contiguous ranges are passed on as one
span and other ranges in 64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never
summed: pass text as `std::string_view`.

Every state value is valid, so nothing has preconditions: `word_offset` is taken modulo 4.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a word; the result is the same as for one call.
- **Zero:** the empty message and an all-zero message of any length give `{0, 0, 0, 0}`, so leading zero words do not
  change the checksum.
- **Wraparound:** the sums wrap modulo 2^64, as in ZFS; there is no length limit.
- **Byte order:** words are read little-endian on every target. The result is four numbers; ZFS decides how they are
  stored.

## 5. CPU acceleration

The word loop adds one word per step, a dependency chain of four additions. Longer inputs run four interleaved lanes:
lane j sums the words j, j + 4, j + 8, ... as a message of its own, and the four lane results are combined with
weights that follow from the definition. The sums wrap modulo 2^64 in both forms, so the result is exact. The lanes
run in vector registers where a kernel exists, else in portable code:

| Target and flags | Lanes |
| --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | One 256-bit vector per sum, from 128 B |
| x86-64 without AVX2 (SSE2 is always there) | Two 128-bit vectors per sum, from 192 B |
| AArch64, little-endian | NEON, two 128-bit vectors per sum, from 192 B |
| 32-bit Arm with NEON, little-endian | NEON, from 192 B |
| other 64-bit targets (RISC-V, big-endian AArch64), or `CHECKSUM_ACCELERATION=OFF` on a 64-bit target | Portable lanes, from 256 B |
| other 32-bit targets (Cortex-M, 32-bit Arm without NEON, big-endian), or `CHECKSUM_ACCELERATION=OFF` on a 32-bit target | Word loop only |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs a byte-by-byte loop with the same result. On 32-bit targets the 64-bit sums of four portable
lanes do not fit the registers and ran 3× slower than the word loop, so those targets never use them. The measurements
behind each choice are in [design/acceleration.md](design/acceleration.md#fletcher4).

## 6. Performance

### 6.1 x86-64: Core Ultra 7 155H

MB/s (10⁶ bytes per second), `-O2`, one core, each call timed in a loop over the same buffer. Portable is the library with
`CHECKSUM_ACCELERATION=OFF` (portable lanes from 256 B), SSE2 the default x86-64 flags, AVX2 `-march=native`.

| Build | 20 B | 64 B | 128 B | 256 B | 512 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GCC portable | 4 269 | 8 114 | 9 378 | 9 725 | 13 357 | 15 640 | 16 917 | 17 497 |
| GCC SSE2 | 4 402 | 8 233 | 9 756 | 13 545 | 16 092 | 17 811 | 19 318 | 19 703 |
| GCC AVX2 | 4 715 | 9 440 | 12 863 | 18 875 | 21 258 | 26 384 | 29 258 | 30 817 |
| Clang portable | 3 665 | 8 977 | 10 443 | 5 591 | 6 096 | 7 818 | 8 424 | 8 757 |
| Clang SSE2 | 4 293 | 8 886 | 10 437 | 13 512 | 16 259 | 17 679 | 19 246 | 19 416 |
| Clang AVX2 | 3 716 | 9 120 | 11 612 | 16 941 | 22 186 | 26 171 | 28 620 | 29 653 |

Below the thresholds every build runs the word loop; differences of up to 10 % there come from code placement, which
moves with the link order. AVX2 is 1.5× the SSE2 kernel at 4 KiB. With GCC the portable lanes are within 15 % of the
SSE2 kernel from 1500 bytes; with Clang they reach about 45 % of it, which matters only for a build with `CHECKSUM_ACCELERATION=OFF`.

Against the OpenZFS kernels (`fletcher_4_native` of OpenZFS f79c81d with its superscalar4, sse2, ssse3 and avx2 kernels,
all built with GCC `-O2 -march=native`), the AVX2 build runs at 0.93× the fastest at 20 bytes, 1.09× at 256 bytes and
1.0× from 1500 bytes.

### 6.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3 and Clang 21, `-O2 -mcpu=cortex-a72`, one core. MB/s:

| Build | 20 B | 64 B | 128 B | 256 B | 512 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AArch64 GCC portable | 803 | 1 343 | 1 526 | 2 228 | 2 432 | 2 797 | 3 026 | 2 766 |
| AArch64 GCC NEON | 803 | 1 337 | 1 531 | 2 209 | 2 450 | 2 827 | 3 024 | 2 757 |
| AArch64 Clang portable | 756 | 1 462 | 1 813 | 1 500 | 1 494 | 1 619 | 1 672 | 1 617 |
| AArch64 Clang NEON | 761 | 1 465 | 1 809 | 2 101 | 2 443 | 2 740 | 2 889 | 2 638 |
| AArch32 GCC word loop | 484 | 778 | 839 | 931 | 1 001 | 1 053 | 1 069 | 1 027 |
| AArch32 GCC NEON | 375 | 678 | 769 | 1 126 | 1 504 | 2 046 | 2 384 | 2 408 |
| AArch32 Clang NEON | 325 | 622 | 785 | 1 354 | 1 788 | 2 460 | 2 862 | 2 815 |

On AArch64 with GCC the portable lanes are as fast as NEON; with Clang NEON is 1.4× them at 256 bytes and 1.7× at
4 KiB. On AArch32 NEON is 2.2× the word loop at 4 KiB with GCC. Against OpenZFS (GCC, its superscalar4 and aarch64_neon
kernels) the AArch64 GCC build runs at 0.92× at 20 bytes, 1.18× at 256 bytes and 1.0× from 4 KiB.

### 6.3 Cortex-M4

Cortex-M cores run the word loop. nRF52840 at 64 MHz, GCC 14.3 `-O2`, code in flash, measured with the cycle counter, MB/s:

| Start | 20 B | 64 B | 256 B | 1500 B | 4 KiB | Cycles per byte at 4 KiB |
| --- | --- | --- | --- | --- | --- | --- |
| Aligned | 7.4 | 12.5 | 16.4 | 17.9 | 18.2 | 3.53 |
| Odd address | 7.0 | 11.4 | 14.5 | 15.7 | 15.9 | 4.03 |

The figures scale with the clock: the STM32L4A6 at 80 MHz runs the same cycles per byte.

## 7. Limitations

- **Weak on short messages and zero words:** leading zero words do not change the checksum, and the sums of a short
  message stay small. Use a CRC where errors must be detected reliably.
- **Little-endian words only:** the equivalent of `fletcher_4_native` on a big-endian host needs its words swapped
  first.
- **No continuation from a stored checksum:** unlike Adler-32 and Fletcher, continuing needs the state, since a
  checksum does not record an unfinished word.
- **No verification helper:** compute the checksum and compare it with the stored one.
- **No RISC-V or big-endian kernels.**
- Code size has not been measured.
