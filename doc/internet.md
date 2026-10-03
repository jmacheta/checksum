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
std::uint16_t value = checksum::internet_compute(std::span(ipv4_header));
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

## 2. API

| Name | What it does |
| --- | --- |
| `internet_state` | The running sum (`sum`) and whether an odd number of bytes was folded (`odd`). A plain value: copy it, compare it, store it. The default is the empty message. |
| `internet_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `internet_finalize(state)` | The checksum: `~state.sum`. |
| `internet_compute(data)` | `internet_finalize(internet_update({}, data))`. |

All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, the same as for the CRC engines. Contiguous ranges are passed
on as one span and other ranges in 64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal
is never summed: pass text as `std::string_view`.

Every `internet_state` value is valid, so nothing has preconditions.

## 3. Behavior

- **Splits:** a message may be split anywhere, including inside a 16-bit word. `odd` records that the next byte is
  the low byte of a word, and the result is the same as for one call.
- **Odd length:** a last odd byte is padded with a zero byte, as RFC 1071 specifies.
- **Zero:** the sum is 0 only if every byte is 0. Otherwise a sum that is a multiple of 0xFFFF is 0xFFFF, so:
  - the empty message and an all-zero message give 0xFFFF;
  - a message that already contains its correct checksum gives 0.

  That is how a receiver verifies a packet.
- **Byte order:** the result is a number; write it into the packet most significant byte first. The library reads
  the message the same way on little- and big-endian targets.

## 4. CPU acceleration

The portable loop adds the widest native word (`std::size_t`) with two independent carry chains. It runs at every
optimization level, and its result does not depend on the target's byte order. The order in which bytes are added
does not change a one's complement sum, so the loop adds native-order words and swaps the bytes of the result once.

| Compiler flags | Kernel | Used from |
| --- | --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | 64-byte blocks: 32-bit halves summed into 64-bit vector lanes | 256 bytes |
| everything else, or `CHECKSUM_ACCELERATION=OFF` | portable loop | always |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs a byte-by-byte loop with the same result. Which other architectures are worth a kernel is
discussed in [design/acceleration.md](design/acceleration.md).

## 5. Performance

All figures are in MB/s (10⁶ bytes per second) and are medians of 5 runs of `checksum_bench_internet` on one core.

### 5.1 x86-64: Core Ultra 7 155H, `-O2`

AVX2 is with `-march=native`; portable is with `CHECKSUM_ACCELERATION=OFF` and default flags.

| Input | GCC 16 portable | GCC 16 AVX2 | Clang 21 portable | Clang 21 AVX2 |
| --- | --- | --- | --- | --- |
| 20 B (IPv4 header) | 6 247 | 5 877 | 6 554 | 6 042 |
| 64 B | 16 245 | 14 949 | 19 185 | 16 963 |
| 256 B | 32 555 | 29 282 | 35 606 | 35 587 |
| 1500 B (Ethernet payload) | 38 431 | 61 740 | 36 038 | 69 702 |
| 4 KiB | 47 651 | 71 706 | 46 800 | 82 518 |
| 1 MiB | 52 230 | 62 414 | 50 919 | 75 445 |

Below 256 bytes both builds run the same portable loop, and the differences are within run-to-run noise. From
1500 bytes the AVX2 kernel is 1.5-1.9× faster. At 1 MiB the gain is smaller, because the data comes from the L2
cache.

### 5.2 Code size

`sum_loop`, the whole run-time code:

| Target | Size |
| --- | --- |
| Cortex-M4, `-Os` | 254 B |
| Cortex-M4, `-O2` | 340 B |
| x86-64, `-O2` | 467 B |
| x86-64, `-O2 -mavx2` | 832 B |

The Cortex-M4 and Arm application cores have not been measured on hardware yet.

## 6. Limitations

- **No in-place update helper:** RFC 1624 updates a checksum after one 16-bit-aligned field changes. The public API
  already does it: start from `internet_state{.sum = static_cast<std::uint16_t>(~old_checksum)}`, fold the bitwise
  complement of the old field, then the new field, and finalize.
- **UDP zero:** the UDP rule of sending 0xFFFF instead of a computed 0 is protocol logic and stays in the caller.
- **No verification helper:** to verify a packet, compute over it, checksum included, and compare the result with 0.
