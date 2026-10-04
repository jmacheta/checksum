# Internet checksum user guide

`checksum` computes the Internet checksum of RFC 1071: the 16-bit one's complement of the one's complement sum of a
message read as 16-bit big-endian words. IPv4, ICMP, IGMP, UDP and TCP use it. It works at compile time or at run
time, and everything is in one header, `checksum/internet.hpp`.

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. It has no tables,
allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/internet.hpp>

// One call. The result goes into the packet most significant byte first.
std::uint16_t value = checksum::internet_compute(std::span(header));
header[10] = std::byte(value >> 8);
header[11] = std::byte(value & 0xFF);

// Incremental, e.g. a UDP pseudo-header, the UDP header and the payload.
checksum::internet_state state;
state = checksum::internet_update(state, pseudo_header);
state = checksum::internet_update(state, udp_header);
state = checksum::internet_update(state, payload);
std::uint16_t udp_checksum = checksum::internet_finalize(state);

// Compile time.
static_assert(checksum::internet_compute(std::string_view("123456789")) == 0xF62A);
```

`examples/internet` has complete programs: an IPv4 header template with its checksum filled in at compile time, an
IPv4 header checksum filled in and verified, a UDP checksum summed from the pseudo-header, the header and the payload,
and the RFC 1624 update of a header after a TTL decrement.

## 2. Definition

The message is read as 16-bit big-endian words, a last odd byte padded with a zero byte. The words are added in one's
complement arithmetic, each carry out of bit 15 added back into bit 0, and the checksum is the one's complement
(bitwise NOT) of that sum.

## 3. API

| Name | What it does |
| --- | --- |
| `internet_state` | The running sum (`sum`) and whether an odd number of bytes was folded (`odd`). A plain value: copy it, compare it, store it. The default is the empty message. |
| `internet_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `internet_update(checksum, data)` | Folds `data` into the message whose checksum is `checksum`, a `std::uint16_t`, and returns the new checksum. That message must have an even length. |
| `internet_finalize(state)` | The checksum: `~state.sum`. |
| `internet_compute(data)` | `internet_finalize(internet_update({}, data))`. |

All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. Contiguous ranges are passed
on as one span and other ranges in 64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal
is never summed: pass text as `std::string_view`.

Every `internet_state` value is valid, so nothing has preconditions.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a 16-bit word. `odd` records that the next byte is
  the low byte of a word, and the result is the same as for one call.
- **Zero:** the sum is 0 only if every byte is 0. Otherwise a sum that is a multiple of 0xFFFF is 0xFFFF, so:
  - the empty message and an all-zero message give 0xFFFF;
  - a message that already contains its correct checksum gives 0.

  That is how a receiver verifies a packet.
- **Byte order:** the result is a number; write it into the packet most significant byte first. The library reads
  the message the same way on little- and big-endian targets.

**Continuing from a stored checksum.** `internet_update(value, data)` continues a message whose checksum is `value`
and returns the new checksum; the running sum is `~value`. It is correct only if that message has an even number of
bytes: the checksum does not record an odd last byte, which `internet_state` keeps in `odd`, so after an odd length
keep the state instead. `value` must be a `std::uint16_t`: another integer type does not compile, so that
`internet_update({}, data)` still starts from the empty state.

## 5. CPU acceleration

The portable loop adds the widest native word (`std::size_t`) with two independent carry chains. It runs at every
optimization level, and its result does not depend on the target's byte order. The order in which bytes are added
does not change a one's complement sum, so the loop adds native-order words and swaps the bytes of the result once.

| Target and flags | Kernel | Used from |
| --- | --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | 64-byte blocks: 32-bit halves summed into 64-bit vector lanes | 512 B |
| AArch64, little-endian (NEON is always there) | NEON pairwise add-accumulate (`uadalp`) of 32-bit words into 64-bit lanes | 512 B |
| 32-bit Arm with NEON, little-endian | the same NEON kernel | 192 B |
| 32-bit Arm without NEON, Thumb-2 or Arm state, e.g. Cortex-M3/M4/M7/M33 or ARMv7-A without NEON | `ldm` + `adcs` carry chain in inline assembly, 32 bytes per iteration, any start address | 192 B |
| RISC-V RV64 with the V extension (`-march=rv64gcv`), any vector length | widening vector add (`vwaddu`) of 32-bit words into 64-bit lanes | 64 B |
| everything else (Thumb-1 code such as Cortex-M0/M0+/M23, big-endian NEON, RV64 without V), or `CHECKSUM_ACCELERATION=OFF` | portable loop | always |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. AArch64 and
Cortex-M3/M4/M7/M33 builds get their kernel from their usual `-march` or `-mcpu` alone. Constant evaluation always runs a
byte-by-byte loop with the same result. Which other architectures are worth a kernel is discussed in
[design/acceleration.md](design/acceleration.md).

## 6. Performance

All figures are in MB/s (10⁶ bytes per second) and are medians of 5 runs of `checksum_bench_internet` on one core.

### 6.1 x86-64: Core Ultra 7 155H, `-O2`

AVX2 is with `-march=native`; portable is with `CHECKSUM_ACCELERATION=OFF` and default flags.

| Input | GCC 16 portable | GCC 16 AVX2 | Clang 21 portable | Clang 21 AVX2 |
| --- | --- | --- | --- | --- |
| 20 B (IPv4 header) | 8 788 | 8 325 | 7 822 | 7 383 |
| 64 B | 19 619 | 19 412 | 22 743 | 19 761 |
| 256 B | 36 378 | 38 476 | 38 666 | 39 792 |
| 1500 B (Ethernet payload) | 39 273 | 64 339 | 37 892 | 69 386 |
| 4 KiB | 49 066 | 78 821 | 48 414 | 83 060 |
| 1 MiB | 54 418 | 68 182 | 53 725 | 77 821 |

Below 512 bytes both builds run the portable loop; the differences come from `-march=native` and code layout. From
1500 bytes the AVX2 kernel is 1.6-1.8× faster. At 1 MiB the gain is smaller, because the data comes from the L2 cache.

### 6.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, static binaries on one core. Portable is with `CHECKSUM_ACCELERATION=OFF`. In 32-bit mode, NEON is
`-march=armv8-a+crc -mfpu=neon-fp-armv8`, and `ldm` is `-mfpu=vfpv3-d16` (no NEON).

| Input | AArch64 portable | AArch64 NEON | AArch32 portable | AArch32 NEON | AArch32 `ldm` |
| --- | --- | --- | --- | --- | --- |
| 20 B | 1 314 | 1 075 | 665 | 842 | 782 |
| 64 B | 2 719 | 3 339 | 1 475 | 2 058 | 1 941 |
| 256 B | 5 360 | 6 042 | 2 293 | 3 339 | 3 049 |
| 1500 B | 6 636 | 10 312 | 2 630 | 8 289 | 4 561 |
| 4 KiB | 7 214 | 13 182 | 2 737 | 11 149 | 5 359 |
| 1 MiB | 5 386 | 6 679 | 2 580 | 6 253 | 4 561 |

The 64-bit portable loop is already fast, so the NEON kernel starts at 512 bytes and is 1.5-1.8× faster from 1500
bytes. In 32-bit mode the kernels start at 192 bytes: NEON is 4.1× and `ldm` 2.0× faster at 4 KiB. Below the
thresholds every build runs the portable loop, and the differences there come from code layout.

### 6.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM; the cycle counter gives the best of 5 calls. Offset is the start
address modulo 4: 2 is typical for an IP header behind a 14-byte Ethernet header.

| Input | nRF52840 portable | nRF52840 `ldm` | STM32L4A6 portable | STM32L4A6 `ldm` |
| --- | --- | --- | --- | --- |
| 20 B | 10.3 | 10.0 | 12.9 | 12.5 |
| 64 B | 24.4 | 23.4 | 30.5 | 29.3 |
| 256 B | 40.2 | 49.5 | 50.2 | 61.9 |
| 1500 B | 48.5 | 80.1 | 60.6 | 100.2 |
| 4 KiB | 50.3 | 91.9 | 62.9 | 114.9 |
| 1500 B, offset 2 | 40.8 | 80.4 | 51.0 | 100.5 |
| 4 KiB, offset 1 | 36.1 | 88.7 | 45.2 | 110.9 |

Per byte at 4 KiB, the kernel takes 0.70 cycles against 1.27 for the portable loop: 1.8× faster, 1.2× at 256 bytes,
and up to 2.5× from odd or 2-modulo-4 addresses, where the portable loop's unaligned loads cost more. Both chips run the
same cycles per byte, so the figures scale with the clock. Below 192 bytes the build with the kernel is up to 5 %
slower.

### 6.4 Code size

`sum_loop` and its helpers, the whole run-time code, GCC with `-ffunction-sections`. A kernel build holds the portable
loop twice: once for short inputs, once after the kernel.

| Target | Portable | With kernel |
| --- | --- | --- |
| Cortex-M4, `-Os` | 210 B | 778 B |
| Cortex-M4, `-O2` | 248 B | 944 B |
| x86-64, `-O2` (AVX2 with `-mavx2`) | 419 B | 1 229 B |
| AArch64, `-O2` | 364 B | 992 B |
| RISC-V RV64, `-O2` (V with `-march=rv64gcv`) | 848 B | 1 136 B |

RISC-V has not been measured on hardware; the vector kernel is tested in QEMU with vector lengths of 128-1024 bits.

## 7. Limitations

- **No in-place update helper:** RFC 1624 updates a checksum after one 16-bit-aligned field changes. The public API
  already does it: continue from the old checksum with the bitwise complement of the old field, then with the new
  field, as `examples/internet/ttl_decrement.cpp` does.
- **UDP zero:** the UDP rule of sending 0xFFFF instead of a computed 0 is protocol logic and stays in the caller.
- **No verification helper:** to verify a packet, compute over it, checksum included, and compare the result with 0.
- **Code size:** a build with a kernel adds 290-810 bytes; `CHECKSUM_ACCELERATION=OFF` keeps the portable loop alone.
