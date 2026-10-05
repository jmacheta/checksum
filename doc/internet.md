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

The portable loop adds the widest native word (`std::size_t`) in 64-byte blocks (32 bytes on 32-bit targets) with two
independent carry chains of four words; on x86-64 each chain is an `add`/`adc` sequence with its carry added back at
once. The bytes after the last whole word come from one load of the message's last word, the bytes already summed
masked off, so no load reaches past the message. It runs at every optimization level, and its result does not depend
on the target's byte order. The order in which bytes are added does not change a one's complement sum, so the loop
adds native-order words and swaps the bytes of the result once.

| Target and flags | Kernel | Used from |
| --- | --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | 64-byte blocks: 32-bit halves summed into 64-bit vector lanes | 512 B |
| AArch64, little-endian (NEON is always there) | NEON pairwise add-accumulate (`uadalp`) of 32-bit words into 64-bit lanes | 512 B |
| 32-bit Arm with NEON, little-endian | the same NEON kernel | 192 B |
| 32-bit Arm without NEON, Thumb-2 or Arm state, e.g. Cortex-M3/M4/M7/M33 or ARMv7-A without NEON | `ldm` + `adcs` carry chain in inline assembly, 32 bytes per iteration, any start address | 192 B |
| RISC-V RV64 with the V extension (`-march=rv64gcv`) or Zve64x, any vector length | widening vector add (`vwaddu`) of 32-bit words into 64-bit lanes | 64 B |
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
| 20 B (IPv4 header) | 8 198 | 8 193 | 7 564 | 7 573 |
| 64 B | 25 213 | 24 161 | 24 291 | 23 353 |
| 256 B | 46 748 | 46 352 | 45 065 | 45 027 |
| 1500 B (Ethernet payload) | 53 538 | 74 313 | 52 868 | 81 358 |
| 4 KiB | 55 492 | 82 858 | 56 057 | 95 187 |
| 1 MiB | 59 974 | 71 025 | 60 008 | 88 316 |

Below 512 bytes both builds run the portable loop; the differences come from `-march=native` and code layout. From
1500 bytes the AVX2 kernel is 1.4-1.7× faster up to 4 KiB, and 1.2× (GCC) or 1.5× (Clang) at 1 MiB.

Against Linux v7.3-rc5 `csum_partial` and DPDK `rte_raw_cksum`, built with the same GCC 16 `-O2 -march=native` and
called the same way, this build runs at 0.97× the faster of the two at 256 B, 0.98× at 64 B, 1.14× at 20 B and
1.35-1.76× from 1500 B.

### 6.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, static binaries on one core. Portable is with `CHECKSUM_ACCELERATION=OFF`. AArch64 is
`-march=armv8-a`; in 32-bit mode, NEON is `-march=armv8-a+crc -mfpu=neon-fp-armv8`, and `ldm` is `-mfpu=vfpv3-d16`
(no NEON).

| Input | AArch64 portable | AArch64 NEON | AArch32 portable | AArch32 NEON | AArch32 `ldm` |
| --- | --- | --- | --- | --- | --- |
| 20 B | 1 190 | 1 144 | 899 | 842 | 837 |
| 64 B | 2 974 | 2 758 | 1 779 | 1 730 | 1 730 |
| 256 B | 5 682 | 5 437 | 2 819 | 4 426 | 3 536 |
| 1500 B | 7 288 | 11 204 | 3 186 | 9 409 | 4 838 |
| 4 KiB | 7 527 | 13 620 | 3 309 | 11 777 | 5 413 |
| 1 MiB | 5 454 | 6 975 | 3 032 | 6 777 | 4 679 |

The 64-bit portable loop is already fast, so the NEON kernel starts at 512 bytes and is 1.5-1.8× faster at
1500 B-4 KiB (1.28× at 1 MiB). In 32-bit mode the kernels start at 192 bytes: NEON is 3.6× and `ldm` 1.6× faster at
4 KiB. Below the thresholds every build runs the portable loop, and the differences there come from code layout.

In the comparison harness, whose figures are lower than this table's, AArch64 takes 957 MB/s at 20 B, 1.10× Linux
v7.3-rc5 `do_csum`, which reads whole 8-byte words and masks off the bytes past the end.

### 6.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM; the cycle counter gives the best of 5 calls. Offset is the start
address modulo 4: 2 is typical for an IP header behind a 14-byte Ethernet header.

| Input | nRF52840 portable | nRF52840 `ldm` | STM32L4A6 portable | STM32L4A6 `ldm` |
| --- | --- | --- | --- | --- |
| 20 B | 10.8 | 10.5 | 13.4 | 13.1 |
| 64 B | 23.3 | 22.6 | 29.1 | 28.3 |
| 256 B | 39.4 | 52.0 | 49.2 | 65.0 |
| 1500 B | 48.9 | 82.6 | 61.1 | 103.4 |
| 4 KiB | 50.3 | 92.6 | 62.8 | 115.7 |
| 1500 B, offset 2 | 41.0 | 81.1 | 51.3 | 101.9 |
| 4 KiB, offset 1 | 36.1 | 90.2 | 45.1 | 112.8 |

Per byte at 4 KiB, the kernel takes 0.69 cycles against 1.27 for the portable loop: 1.8× faster, 1.3× at 256 bytes,
and up to 2.5× from odd or 2-modulo-4 addresses, where the portable loop's unaligned loads cost more. Both chips run the
same cycles per byte, so the figures scale with the clock. Below 192 bytes both builds run the portable loop: 191 bytes
take 346 cycles against 273 for the kernel at 192. The kernel breaks even at 120-160 bytes, depending on the start
address, so the threshold could move lower.

### 6.4 Code size

`sum_loop` and its helpers, the whole run-time code, GCC with `-ffunction-sections`. A kernel build holds the portable
loop twice: once for short inputs, once after the kernel.

| Target | Portable | With kernel |
| --- | --- | --- |
| Cortex-M4, `-Os` | 284 B | 920 B |
| Cortex-M4, `-O2` | 356 B | 1 104 B |
| x86-64, `-O2` (AVX2 with `-mavx2`) | 385 B | 985 B |
| AArch64, `-O2` | 484 B | 1 076 B |
| RISC-V RV64, `-O2` (V with `-march=rv64gcv`) | 1 620 B | 1 402 B |

RV64 without fast unaligned access loads each word byte by byte, which makes its unrolled portable loop large.

RISC-V has not been measured on hardware. The preset tests the vector kernel at QEMU's default vector length of 128
bits; 256-1024 bits were tested by hand, and 64 bits (Zve64x) only by reasoning, since QEMU 10.2 user mode crashes there.

## 7. Limitations

- **No in-place update helper:** RFC 1624 updates a checksum after one 16-bit-aligned field changes. The public API
  already does it: continue from the old checksum with the bitwise complement of the old field, then with the new
  field, as `examples/internet/ttl_decrement.cpp` does.
- **UDP zero:** the UDP rule of sending 0xFFFF instead of a computed 0 is protocol logic and stays in the caller.
- **No verification helper:** to verify a packet, compute over it, checksum included, and compare the result with 0.
- **Code size:** a build with a kernel adds 590-750 bytes on Arm and x86-64; `CHECKSUM_ACCELERATION=OFF` keeps the portable loop alone.
