# Adler-32 user guide

`checksum` computes Adler-32, the checksum of the zlib format (RFC 1950), at compile time or at run time. Its values
are those of zlib's `adler32()`. Everything is in one header, `checksum/adler32.hpp`.

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. It has no tables,
allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/adler32.hpp>
using namespace std::literals;

// One call.
std::uint32_t value = checksum::adler32_compute(std::span(buffer));

// Incremental: a message may be split anywhere.
checksum::adler32_state state;
state = checksum::adler32_update(state, first_part);
state = checksum::adler32_update(state, second_part);
std::uint32_t stream_checksum = checksum::adler32_finalize(state);

// Continue from a stored checksum.
std::uint32_t extended = checksum::adler32_update(stream_checksum, third_part);

// Compile time.
static_assert(checksum::adler32_compute("123456789"sv) == 0x091E01DE);
static_assert(checksum::adler32_compute("Wikipedia"sv) == 0x11E60398);
```

`examples/adler32` has complete programs: the checksum of default settings computed at compile time, checking the
Adler-32 trailer of a zlib stream, and extending a stored checksum when data is appended.

## 2. Definition

Adler-32 keeps two sums modulo 65521, the largest prime below 2^16: `sum1` starts at 1 and adds each byte, `sum2`
starts at 0 and adds `sum1` after each byte. The checksum is `(sum2 << 16) | sum1`. It differs from Fletcher-32
([fletcher.md](fletcher.md)) in the prime modulus, the start value 1 and the 8-bit blocks.

## 3. API

| Name | What it does |
| --- | --- |
| `adler32_state` | The two sums, `sum1` and `sum2`. A plain value: copy it, compare it, store it. The default (`sum1 = 1`, `sum2 = 0`) is the empty message. |
| `adler32_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `adler32_update(checksum, data)` | Folds `data` into the message whose checksum is `checksum`, a `std::uint32_t`, and returns the new checksum. |
| `adler32_finalize(state)` | The checksum. |
| `adler32_compute(data)` | `adler32_finalize(adler32_update({}, data))`. |

All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. Contiguous ranges are passed on as
one span and other ranges in 64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never
summed: pass text as `std::string_view`.

Every state value is valid, so nothing has preconditions: sums from 65521 up count modulo 65521.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Empty message:** gives 1.
- **Byte order:** the result is a number; zlib stores it most significant byte first.

**Continuing from a stored checksum.** The checksum holds both sums, so `adler32_update(value, data)` continues a
message whose checksum is `value`, for example one computed by zlib, after any number of bytes, and returns the new
checksum, as zlib's `adler32(adler, buf, len)` does. `value` must be a `std::uint32_t`: another integer type does not
compile, so that `adler32_update({}, data)` still starts from the empty state, not from checksum 0.

## 5. CPU acceleration

The portable loop adds bytes into `std::size_t` sums and reduces them only when an overflow could otherwise happen:
after 380 million bytes on 64-bit targets, after 5 552 bytes on 32-bit targets. It adds four bytes per step, so
`sum1` has one addition per four bytes in its dependency chain, and it reduces without a division, since 2^16 is 15
modulo 65521.

From 64 bytes, Adler-32 runs the byte kernel of Fletcher-16 with modulus 65521:

| Target and flags | Kernel |
| --- | --- |
| x86-64 with AVX-VNNI (`-mavxvnni`, or a `-march` that includes it, e.g. Alder Lake, Meteor Lake) | `vpdpbusd`, 128 bytes per iteration |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | 32-byte vectors |
| x86-64 without AVX2 (SSE2 is always there; SSSE3 used if enabled) | 16-byte vectors |
| AArch64 with NEON, little-endian | NEON, 16-byte vectors; from 320 bytes 64 bytes per iteration |
| 32-bit Arm with NEON, little-endian | NEON, 16-byte vectors |
| Little-endian M-profile Arm with the DSP extension (Cortex-M4/M7/M33) | `usada8` + `smlad`, 8 bytes per iteration |
| everything else, or `CHECKSUM_ACCELERATION=OFF` | portable loop |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs a byte-by-byte loop with the same result. The measurements behind each choice are in
[design/acceleration.md](design/acceleration.md#fletcher-and-adler-32).

## 6. Performance

### 6.1 x86-64: Core Ultra 7 155H

GCC 16, 4 KiB, one pinned core, the better median of two runs: 7 309 MB/s (10⁶ bytes per second) for the portable
loop, 22 964 MB/s with SSE2 and 58 088 MB/s with the AVX2 kernel, 7.9× faster. With `-march=native` the AVX-VNNI
kernel runs (MB/s, GCC 16 and Clang 21):

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| GCC | 4 074 | 11 570 | 35 225 | 56 789 | 84 504 | 88 630 |
| Clang | 3 746 | 11 655 | 36 649 | 57 952 | 85 027 | 89 914 |

In a separate comparison harness, against zlib-ng (develop, its AVX-VNNI kernel, same flags) the library runs 0.98×
at 20 B, 0.78× at 64 B, 0.91× at 256 B and 1.13-1.17× from 4 KiB; the AVX2 kernel reached 0.71-0.83× there.

### 6.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, AArch64 `-march=armv8-a`, one core, start address aligned. GiB/s:

| Build | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- |
| AArch64 portable | 0.69 | 0.77 | 0.83 | 0.84 |
| AArch64 NEON | 1.15 | 2.60 | 5.01 | 6.16 |
| AArch32 portable | 0.66 | 0.77 | 0.85 | 0.87 |
| AArch32 NEON | 0.95 | 2.42 | 3.75 | 4.30 |

NEON is 5.0-7.3× faster at 4 KiB and 1.4-1.7× at 64 bytes. On AArch64, from 320 bytes the kernel sums 64 bytes per
iteration: 0.96-0.99× zlib-ng and ISA-L from 1500 B in the comparison harness, where 16 bytes per iteration reached
0.72-0.79×.

### 6.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM, measured with the cycle counter. At 4 KiB the portable loop takes
5.29 cycles per byte and the DSP kernel 2.68 (2.70 from an odd address). Both chips run the same cycles per byte, so
the figures scale with the clock. STM32L4A6, MB/s:

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Portable | 6.6 | 10.7 | 12.6 | 13.4 | 13.8 | 14.9 | 15.1 |
| DSP kernel | 6.8 | 12.9 | 18.1 | 20.9 | 22.7 | 28.5 | 29.8 |

The kernel is 2.0× faster at 4 KiB and 1.6× at 256 bytes.

## 7. Limitations

- **Weak on short messages:** with few bytes `sum1` stays far below 65521, so most of the 32 bits are unused. Use a CRC
  for short messages where errors must be detected reliably.
- **No combination** of two checksums (`adler32_combine` in zlib).
- **No verification helper:** compute the checksum and compare it with the stored one.
- **No RISC-V or big-endian kernels.**
- **Untested reduction bound:** on 64-bit targets the portable loop reduces after 380 368 439 bytes, a span no test
  reaches (the longest test is 600 KiB).
- Code size: Cortex-M4, GCC 14.3: the library object with the out-of-line loops of all sizes takes 820 B of code at `-O2` and 662 B at `-Os`; the inline short paths of the header add to each caller.
