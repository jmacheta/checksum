# Fletcher checksum user guide

`checksum` computes Fletcher-16, Fletcher-32 and Fletcher-64, at compile time or at run time. Everything is in one
header, `checksum/fletcher.hpp`. Adler-32, a close relative with its own modulus, has its own guide:
[adler32.md](adler32.md).

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. It has no tables,
allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/fletcher.hpp>
using namespace std::literals;

// One call.
std::uint32_t value = checksum::fletcher32_compute(std::span(buffer));

// Incremental: a message may be split anywhere.
checksum::fletcher16_state state;
state = checksum::fletcher_update(state, header);
state = checksum::fletcher_update(state, payload);
std::uint16_t frame_checksum = checksum::fletcher_finalize(state);

// Compile time.
static_assert(checksum::fletcher16_compute("abcde"sv) == 0xC8F0);
static_assert(checksum::fletcher32_compute("abcde"sv) == 0xF04FC729);
static_assert(checksum::fletcher64_compute("abcde"sv) == 0xC8C6C527646362C6);
```

`examples/fletcher` has complete programs: checksums at compile time, a file checksum computed in chunks, and a frame
check.

## 2. Definition

Fletcher-N reads the message as blocks of N/2 bits and keeps two sums modulo M = 2^(N/2) − 1, both starting at 0:
`sum1` adds each block, `sum2` adds `sum1` after each block. The checksum is `(sum2 << N/2) | sum1`.

| Checksum | Block | M | Result |
| --- | --- | --- | --- |
| Fletcher-16 | 1 byte | 255 | `std::uint16_t` |
| Fletcher-32 | 16-bit little-endian word | 65535 | `std::uint32_t` |
| Fletcher-64 | 32-bit little-endian word | 2^32 − 1 | `std::uint64_t` |

A last incomplete block is padded with zero bytes. The values match the examples of the Wikipedia article
"Fletcher's checksum", which reads words in little-endian order.

## 3. API

`Width` is 16, 32 or 64. `fletcher_update` and `fletcher_finalize` take the width from the state or the checksum
type, so only the state and `fletcher_compute` have aliases per width.

| Name | What it does |
| --- | --- |
| `fletcher_state<Width>` | `sum1`, `sum2` and `block_offset`, the bytes of an unfinished block already in `sum1`. A plain value: copy it, compare it, store it. The default is the empty message. |
| `fletcher_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `fletcher_update(checksum, data)` | Folds `data` into the message whose checksum is `checksum` and returns the new checksum; `std::uint16_t`, `std::uint32_t` or `std::uint64_t` selects the width. That message must end on a whole block. |
| `fletcher_finalize(state)` | The checksum, an unfinished block padded with zero bytes. |
| `fletcher_compute<Width>(data)` | `fletcher_finalize(fletcher_update(fletcher_state<Width>{}, data))`. |
| `fletcher16_state`, `fletcher32_state`, `fletcher64_state` | `fletcher_state<16>`, `fletcher_state<32>`, `fletcher_state<64>`. |
| `fletcher16_compute(data)`, `fletcher32_compute(data)`, `fletcher64_compute(data)` | `fletcher_compute<16>(data)`, `fletcher_compute<32>(data)`, `fletcher_compute<64>(data)`. |

`fletcher_state<Width>::value_type` is the checksum type and `sum_type` the type of one sum (`std::uint8_t`,
`std::uint16_t`, `std::uint32_t`). All functions are `constexpr` and `noexcept`. `data` is a
`std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`, `signed char` or `char8_t`, as for
the other algorithms. Contiguous ranges are passed on as one span and other ranges in 64-byte chunks. Arrays of `char`
are rejected, so the `'\0'` of a string literal is never summed: pass text as `std::string_view`.

Every state value is valid, so nothing has preconditions: a sum equal to M counts as 0, and `block_offset` is taken
modulo the block size.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a block; the result is the same as for one call.
- **Zero:** the empty message and an all-zero message give 0.
- **Ones and zeros:** a sum never equals M, so a block of all one bits counts as a block of zeros. For Fletcher-16,
  bytes 0x00 and 0xFF give the same checksum, and neither byte of the result is ever 0xFF.
- **Byte order:** words are read little-endian on every target, and the result is a number; the protocol decides how
  it goes into a frame.

**Continuing from a stored checksum.** `fletcher_update(value, data)` continues a message whose checksum is `value`
and returns the new checksum. The type of `value` selects the width: `std::uint16_t` for Fletcher-16, `std::uint32_t`
for Fletcher-32, `std::uint64_t` for Fletcher-64. Other integer types, `int` and `unsigned char` included, do not
compile rather than pick a width, so write `std::uint32_t{0xF04FC729}` for a literal; `{}` does not compile either,
name the state type (`fletcher32_state{}`). Fletcher-32 and Fletcher-64 continue correctly only after whole blocks,
an even length or a multiple of 4: the checksum already counts an unfinished block as padded with zeros, so after such
a length keep the state instead. Fletcher-16 continues after any length.

## 5. CPU acceleration

The portable loop adds blocks into the widest native integer and reduces modulo M only when an overflow could
otherwise happen: on 64-bit targets after 380 million bytes for Fletcher-16, on 32-bit targets after 5 802 bytes. It
adds four blocks per step, so `sum1` has one addition per four blocks in its dependency chain. On 32-bit targets
Fletcher-32 sums 32-bit words, two blocks at a time; on AArch64 it sums its 16-bit blocks in 32-bit sums, reduced every
360 blocks.

From a minimum size, a kernel sums whole vectors of blocks. It returns the sum of the blocks and their weighted sum,
and `sum2` gains both at the end of each chunk, 2 KiB to 256 KiB depending on the kernel.

| Target and flags | Fletcher-16 | Fletcher-32 | Fletcher-64 |
| --- | --- | --- | --- |
| x86-64 with AVX-VNNI (`-mavxvnni`, or a `-march` that includes it) | `vpdpbusd`, 128 bytes per iteration, from 64 B | as AVX2 | as AVX2 |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | 32-byte vectors, from 64 B | from 128 B | from 384 B |
| x86-64 without AVX2 (SSE2 is always there; SSSE3 used if enabled) | 16-byte vectors, from 64 B | from 128 B | portable loop |
| AArch64, little-endian | NEON, from 64 B; 64 bytes per iteration from 320 B | from 128 B | from 256 B |
| 32-bit Arm with NEON, little-endian | NEON, from 64 B | from 128 B | from 256 B |
| Little-endian M-profile Arm with the DSP extension (Cortex-M4/M7/M33) | `usada8` + `smlad`, from 64 B | portable loop | portable loop |
| everything else (Cortex-M0/M3, A-profile 32-bit Arm without NEON, big-endian, RISC-V), or `CHECKSUM_ACCELERATION=OFF` | portable loop | portable loop | portable loop |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs a byte-by-byte loop with the same result. The DSP kernel first sums single bytes up to a 4-byte
aligned address, so the start address hardly matters. Adler-32 uses the same byte kernels. The measurements behind
each choice are in [design/acceleration.md](design/acceleration.md#fletcher-and-adler-32).

## 6. Performance

### 6.1 x86-64: Core Ultra 7 155H

MB/s (10⁶ bytes per second) at 4 KiB, GCC 16 and Clang 21, measured on a loaded host, so the figures are approximate.
Portable is the library without kernels, SSE2 the default x86-64 flags, AVX2 a build with AVX2 enabled.

| Checksum | GCC portable | GCC SSE2 | GCC AVX2 | Clang portable | Clang AVX2 |
| --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 7 314 | 22 778 | 54 107 | 3 205 | 58 109 |
| Fletcher-32 | 14 167 | 21 916 | 47 221 | 6 447 | 54 311 |
| Fletcher-64 | 26 853 | | 52 801 | 13 065 | 54 881 |

The AVX2 kernels are 7.4× (Fletcher-16), 3.3× (Fletcher-32) and 2.0× (Fletcher-64) faster than the GCC portable loop.
With `-march=native` (AVX-VNNI), GCC 16, MB/s:

| Checksum | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 3 094 | 9 959 | 31 780 | 54 938 | 81 283 | 90 798 |
| Fletcher-32 | 3 972 | 7 739 | 21 029 | 37 057 | 49 934 | 51 822 |
| Fletcher-64 | 3 920 | 9 423 | 18 352 | 37 030 | 49 787 | 59 639 |

At 20 bytes the plain deferred-modulo loop of the Wikipedia article (`-O2 -march=native`) runs 3 300 MB/s for
Fletcher-16 and 4 600 for Fletcher-32: 0.94× and 0.86× for the library, which needs the state and its unfinished
block on top.

### 6.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, one core. GiB/s at 64 B / 256 B / 1500 B / 4 KiB, start address aligned.

| Checksum | AArch64 portable | AArch64 NEON | AArch32 portable | AArch32 NEON |
| --- | --- | --- | --- | --- |
| Fletcher-16 | 0.62 / 0.74 / 0.83 / 0.84 | 0.96 / 2.38 / 4.67 / 5.75 | 0.57 / 0.73 / 0.82 / 0.84 | 0.82 / 2.13 / 3.58 / 4.23 |
| Fletcher-32 | 0.98 / 1.42 / 1.61 / 1.67 | 1.09 / 2.57 / 4.27 / 4.78 | 0.78 / 1.37 / 1.56 / 1.65 | 0.80 / 2.37 / 4.09 / 4.74 |
| Fletcher-64 | 1.46 / 2.71 / 3.36 / 3.56 | 1.41 / 2.85 / 4.41 / 5.16 | 0.91 / 1.40 / 1.61 / 1.67 | 0.90 / 2.30 / 4.10 / 5.06 |

At 4 KiB NEON is 5.0× (AArch32) to 6.8× (AArch64, 64 bytes per iteration) faster for Fletcher-16, 2.8-2.9× for Fletcher-32, and 1.45× (AArch64) or 3.0× (AArch32) for
Fletcher-64, whose 64-bit portable loop is already fast on AArch64. Below the thresholds both builds run the portable
loop; the differences there come from code layout. At 20 bytes AArch64 Fletcher-32 runs 592 MB/s, 0.88× the Wikipedia
loop (675).

### 6.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM, measured with the cycle counter. Both chips run the same cycles per
byte, so the figures scale with the clock. Cycles per byte at 4 KiB:

| Checksum | Portable | DSP kernel | Kernel build, start address odd |
| --- | --- | --- | --- |
| Fletcher-16 | 5.30 | 2.69 | 2.71 |
| Fletcher-32 | 2.11 | (portable) | 2.61 |
| Fletcher-64 | 1.80 | (portable) | 2.30 |

Fletcher-16 on the STM32L4A6, MB/s:

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Portable | 5.9 | 10.2 | 12.2 | 13.0 | 13.5 | 14.9 | 15.1 |
| DSP kernel | 5.9 | 12.1 | 17.3 | 20.2 | 22.1 | 28.2 | 29.8 |

The kernel is 2.0× faster at 4 KiB and 1.6× at 256 bytes. On this core the wider sums are cheaper per byte:
Fletcher-64 at 1.80 cycles per byte is the fastest of the three, and Fletcher-32 and Fletcher-64 are slower from an
odd address, where their word loads are unaligned.

## 7. Limitations

- **Weaker error detection than a CRC:** a Fletcher checksum cannot tell a block of zeros from a block of ones. Use a
  CRC where errors must be detected reliably.
- **Little-endian words only:** a protocol that reads Fletcher-32 or Fletcher-64 blocks big-endian needs its bytes
  swapped first.
- **No verification helper:** compute the checksum and compare it with the received one.
- **No RISC-V or big-endian kernels**, and no Fletcher-64 kernel without AVX2 on x86-64.
- Code size has not been measured.
