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

## 6. Performance guidelines

Measured figures are in [performance.md](performance.md#internet-checksum).

- **Kernels start at a minimum size** (section 5): 512 B on x86-64 and AArch64, 192 B on 32-bit Arm, 64 B on RISC-V.
  Packet headers are shorter and always run the portable loop, which is already fast: about 8 000 MB/s for a 20-byte
  IPv4 header on x86-64.
- **On x86-64, enable AVX2** (`-mavx2` or a `-march` that includes it): from 1500 B it makes the checksum 1.4-1.7×
  faster. AArch64 and Cortex-M3/M4/M7/M33 get their kernel from their usual flags.
- **On Cortex-M, the start address matters for the portable loop only.** It is up to 2.5× slower than the kernel from
  an odd or 2-modulo-4 address, typical for an IP header behind a 14-byte Ethernet header. The kernel runs at the
  same speed from any address.
- **Flash budget:** a kernel adds 590-750 bytes of code on Arm and x86-64. `CHECKSUM_ACCELERATION=OFF` keeps the
  portable loop alone.

## 7. Limitations

- **UDP zero:** the UDP rule of sending 0xFFFF instead of a computed 0 is protocol logic and stays in the caller.
