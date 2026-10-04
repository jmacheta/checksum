# xxHash user guide

`checksum` computes the non-cryptographic hashes XXH32, XXH64, XXH3-64 and XXH3-128 with a seed, at compile time or at
run time. The values are those of the reference implementation (`xxhash.h` 0.8.4) on every target. XXH32 and XXH64 are
in `checksum/xxhash.hpp`, XXH3-64 and XXH3-128 in `checksum/xxh3.hpp`.

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. Nothing allocates
memory and there is no global state; XXH3 keeps its 192-byte default secret in read-only data.

## 1. Quick start

```cpp
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>
using namespace std::literals;

// One call, seed 0 or another seed.
std::uint32_t small = checksum::xxh32_compute(key);
std::uint64_t hash = checksum::xxh3_compute<64>(std::span(key), 1234);
checksum::xxh3_hash128 wide = checksum::xxh3_compute<128>(std::span(key));

// Incremental: a message may be split anywhere.
checksum::xxh3_state<64> state{.seed = 1234};
state = checksum::xxh3_update(state, first_part);
state = checksum::xxh3_update(state, second_part);
std::uint64_t value = checksum::xxh3_finalize(state);

// Compile time.
static_assert(checksum::xxh32_compute("abc"sv) == 0x32D153FF);
static_assert(checksum::xxh64_compute("abc"sv) == 0x44BC2CF5AD770999);
static_assert(checksum::xxh3_compute<64>("abc"sv) == 0x78AF5F94892F3950);
static_assert(checksum::xxh3_compute<128>("abc"sv) ==
              checksum::xxh3_hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});
```

## 2. API

Both families have the same four names with their own prefix: a state, `update`, `finalize` and `compute`. All
functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. `compute` hashes contiguous ranges
directly, without the state's buffer; `update` passes contiguous ranges on as one span and folds other ranges in
64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never hashed: pass text as
`std::string_view`.

Every state value is valid, so nothing has preconditions. A state is a plain value: copy it, store it, and hash one
prefix of a message several ways.

### 2.1 XXH32 and XXH64

`Width` is 32 for XXH32 and 64 for XXH64.

| Name | What it does |
| --- | --- |
| `xxhash_state<Width>` | The four lane accumulators, the number of bytes folded (`length`), the `seed` and the bytes of an unfinished stripe (`buffer`, 16 or 32 bytes). The default is the empty message with seed 0; `xxhash_state<Width>{.seed = seed}` starts one with another seed. |
| `xxhash_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxhash_finalize(state)` | The hash of the message folded into `state`; the state is unchanged. |
| `xxhash_compute<Width>(data, seed = 0)` | `xxhash_finalize(xxhash_update(xxhash_state<Width>{.seed = seed}, data))`. |
| `xxh32_state`, `xxh64_state` | `xxhash_state<32>` and `xxhash_state<64>`. |
| `xxh32_compute(data, seed = 0)`, `xxh64_compute(data, seed = 0)` | `xxhash_compute<32>` and `xxhash_compute<64>`. |

`xxhash_state<Width>::value_type` is the type of the hash, the seed and the lanes: `std::uint32_t` for XXH32 and
`std::uint64_t` for XXH64. The lanes are set from the seed when the first whole stripe is folded; before that they are
ignored.

`examples/xxhash` has complete programs: message IDs hashed at compile time, the XXH64 of a file read in chunks, and
a cache keyed by the hash of its input.

### 2.2 XXH3-64 and XXH3-128

`Width` is 64 for XXH3-64 and 128 for XXH3-128.

| Name | What it does |
| --- | --- |
| `xxh3_state<Width>` | The eight accumulators, the number of bytes passed (`length`), the `seed` and a 256-byte `buffer`: 336 bytes. The default is the empty message with seed 0; `xxh3_state<Width>{.seed = seed}` starts one with another seed. |
| `xxh3_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxh3_finalize(state)` | The hash of the message folded into `state`; the state is unchanged. |
| `xxh3_compute<Width>(data, seed = 0)` | `xxh3_finalize(xxh3_update(xxh3_state<Width>{.seed = seed}, data))`. |
| `xxh3_hash128` | The XXH3-128 hash: `low` and `high`, its lower and upper 64 bits, as `XXH128_hash_t` of the reference. Compares with `==`. |

`xxh3_state<Width>::value_type` is `std::uint64_t` for Width 64 and `xxh3_hash128` for Width 128. The seed is a
`std::uint64_t` for both. The hashes always use the default secret; a custom secret cannot be passed.

The state keeps the last 1 to 256 bytes passed in its buffer and folds the rest into the accumulators, so it can
hash the end of the message as the one-shot function does. Messages up to 240 bytes are hashed from the buffer alone.

## 3. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes. XXH3 with seed 0 hashes with the default secret; another seed
  derives a 192-byte secret from it on the stack, for each call that folds stripes (a one-shot hash of more than
  240 bytes, an `xxh3_update` that folds its buffer, or `xxh3_finalize` of more than 240 bytes).
- **Byte order:** input words are read little-endian on every target, so a big-endian target gives the same hashes.
  The result is a number or a pair of numbers. The canonical form of the reference writes a hash most significant byte
  first, and for XXH3-128 `high` before `low`.
- **Long messages:** the length is a 64-bit count; XXH32 mixes it in modulo 2^32, as the reference does.
- **Streaming cost:** `xxh3_update` takes the state by value and returns it, which copies its 336 bytes twice per
  call. Folding works in place, so the cost is per call, not per byte: in 64-byte pieces XXH3 streams at 2.4 GiB/s
  against 9.3 GiB/s for an in-place update (x86-64, SSE2 kernel), about 4× slower; from 4 KiB pieces the copy is
  negligible. Hash short pieces into a buffer of your own, or call `xxh3_compute` once when the whole message is in
  memory. With a seed other than 0, each update that folds its buffer builds the secret again: about 25 % slower than
  seed 0 in 256-byte pieces, 6 % in 4 KiB pieces, on x86-64. The `byte_range` overload of `xxh3_update` folds its
  chunks into one state without copying it.

## 4. CPU acceleration

### 4.1 XXH32 and XXH64: no kernel

XXH32 and XXH64 have no kernel, on any target. Each of the four lanes is a serial chain of a multiplication and a
rotation per word; the scalar loop already keeps four of them in flight. A NEON prototype of XXH32 ran 1 685 MB/s on a
Cortex-A72 at 4 KiB against 2 333 for the scalar loop, and Clang vectorizing the lanes on its own lost 38 % on
x86-64. XXH64 needs 64-bit multiplications per lane, which SSE2, AVX2 and NEON do not have. The stripe loop is
compiled once per width in `src/xxhash/stripe_loop.cpp`, out of line, so that compilers keep the lanes scalar; the
one-shot XXH32 of a message of at least 16 bytes runs a function there that keeps them in local variables.

### 4.2 XXH3

Inputs up to 240 bytes run straight-line code with no loop and no kernel. Longer inputs fold 64-byte stripes into eight
64-bit accumulators, and a kernel holds them in vector registers:

| Target and flags | Kernel |
| --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | Two 256-bit vectors, every stripe. |
| x86-64 without AVX2 (SSE2 is always there) | Four 128-bit vectors, every stripe. |
| AArch64, little-endian | Four lanes in NEON and four in scalar registers, from 448 bytes of whole stripes (messages from 512 bytes). |
| 32-bit Arm with NEON, little-endian | All eight lanes in NEON, every stripe. |
| RISC-V RV64 with the V extension, VLEN ≥ 128 | One register group of eight 64-bit lanes, every stripe. Tested in QEMU, not measured on hardware. |
| everything else (Cortex-M, 32-bit Arm without NEON, big-endian, RISC-V without V), or `CHECKSUM_ACCELERATION=OFF` | Portable loop. |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs the portable code with the same result. On AArch64 the portable loop is faster below
512 bytes, and four NEON lanes with four scalar ones beat all eight in NEON. The measurements behind each choice are in
[design/acceleration.md](design/acceleration.md#xxh3).

## 5. Performance

### 5.1 x86-64: Core Ultra 7 155H

XXH3-64, MB/s (10⁶ bytes per second), GCC 16. Portable is the library with `CHECKSUM_ACCELERATION=OFF`, SSE2 the
default x86-64 flags, AVX2 a build with AVX2 enabled.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Portable | 13 269 | 26 667 | 11 060 | 13 165 | 13 932 | 13 809 |
| SSE2 | 12 946 | 26 749 | 16 291 | 21 057 | 22 002 | 21 992 |
| AVX2 | 14 463 | 28 828 | 28 533 | 43 777 | 48 504 | 48 994 |

With Clang 21 at 1 MiB: 20 107 MB/s portable, 32 658 SSE2, 46 907 AVX2. Up to 240 bytes the three builds run the
same code, and 64 bytes are faster than 256 because they need no stripe loop. At 1 MiB AVX2 is 3.5× the
portable loop and SSE2 1.6×. With `-march=native` (AVX2), MB/s at 20 B and 1 MiB: XXH32 6 831 and 8 743, XXH64 7 371 and 17 689, XXH3-128 7 827 and
50 172.

### 5.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, one core, MB/s:

| Hash, build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| XXH32, AArch64 | 857 | 1 730 | 2 521 | 2 808 | 2 916 | 2 656 |
| XXH64, AArch64 | 862 | 942 | 1 553 | 1 874 | 1 949 | 1 891 |
| XXH3-64, AArch64 portable | 1 239 | 2 440 | 2 799 | 4 239 | 4 602 | 4 134 |
| XXH3-64, AArch64 NEON | 1 239 | 2 440 | 2 799 | 4 315 | 4 698 | 4 148 |
| XXH3-128, AArch64 portable | 804 | 1 795 | 2 035 | 3 865 | 4 431 | 4 129 |
| XXH3-128, AArch64 NEON | 804 | 1 796 | 2 014 | 3 867 | 4 452 | 4 156 |
| XXH32, AArch32 | 646 | 1 419 | 2 161 | 2 491 | 2 589 | 2 326 |
| XXH64, AArch32 | 323 | 370 | 594 | 699 | 730 | 724 |
| XXH3-64, AArch32 portable | 712 | 1 275 | 1 144 | 1 687 | 1 827 | 1 772 |
| XXH3-64, AArch32 NEON | 712 | 1 275 | 1 596 | 2 561 | 2 826 | 2 640 |
| XXH3-128, AArch32 portable | 448 | 883 | 928 | 1 588 | 1 783 | 1 778 |
| XXH3-128, AArch32 NEON | 450 | 877 | 1 214 | 2 355 | 2 703 | 2 642 |

On AArch64 the NEON kernel runs from 512 bytes and gains 2 % at 4 KiB over a portable loop that the compiler already
schedules well; on AArch32 NEON is 1.5× faster from 1500 bytes. XXH64 needs 64-bit multiplications, which AArch32
builds from 32-bit ones; there XXH32 is the fastest of the four up to 64 bytes and from 256 bytes on a par with
XXH3-64 NEON.

### 5.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM, measured with the cycle counter. The Cortex-M4 runs the portable code
of every hash. Both chips run the same cycles per byte, so the figures scale with the clock. STM32L4A6, MB/s, and
cycles per byte at 4 KiB:

| Hash | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB | Cycles per byte |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| XXH32 | 9.9 | 23.6 | 32.3 | 36.8 | 39.6 | 48.4 | 50.3 | 1.59 |
| XXH64 | 8.4 | 9.3 | 13.4 | 15.8 | 17.3 | 21.6 | 23.6 | 3.40 |
| XXH3-64 | 10.0 | 19.5 | 22.1 | 19.5 | 13.8 | 23.6 | 26.7 | 3.00 |
| XXH3-128 | 8.5 | 16.7 | 19.2 | 11.2 | 10.7 | 21.7 | 25.7 | 3.11 |

On this core XXH32 is the fastest at every size, about twice as fast as the others from 256 bytes. Speed at short
sizes moves by up to 25 % with where the code lands in flash.

## 6. Limitations

- **Not cryptographic:** none of these hashes resists inputs crafted to collide, even with a secret seed. Do not use
  them for message authentication or to protect hash tables from hostile keys.
- **Default secret only:** XXH3 takes a seed but no custom secret, so the reference's `XXH3_64bits_withSecret` and
  related variants are not provided.
- **Not an error check:** unlike a CRC, they guarantee the detection of no class of errors, burst errors included.
- **Streaming in small pieces is slow for XXH3** (section 3): the state is passed by value.
- **No kernels for XXH32 and XXH64** (section 4.1), and no XXH3 kernel for Cortex-M, big-endian targets or AVX-512.
- Code size has not been measured.
