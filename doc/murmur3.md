# MurmurHash3 user guide

`checksum` computes the non-cryptographic hashes MurmurHash3_x86_32 and MurmurHash3_x64_128 with a 32-bit seed, at
compile time or at run time. The values are those of the reference implementation (`MurmurHash3.cpp` of SMHasher) on
every target. Everything is in one header, `checksum/murmur3.hpp`.

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. It has no tables,
allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/murmur3.hpp>
using namespace std::literals;

// One call, seed 0 or another seed.
std::string_view key = "user:42";
std::uint32_t hash = checksum::murmur3_32_compute(key);
checksum::hash128 wide = checksum::murmur3_128_compute(key, 1234);

// Incremental: a message may be split anywhere.
checksum::murmur3_128_state state{.seed = 1234};
state = checksum::murmur3_update(state, first_part);
state = checksum::murmur3_update(state, second_part);
checksum::hash128 value = checksum::murmur3_finalize(state);

// Compile time.
static_assert(checksum::murmur3_32_compute("Hello, world!"sv, 1234) == 0xFAF6CDB3);
```

`examples/murmur3` has complete programs: command dispatch on hashes computed at compile time, a Bloom filter and
deduplication of records by their 128-bit hash.

## 2. API

`Width` is 32 for MurmurHash3_x86_32 and 128 for MurmurHash3_x64_128.

| Name | What it does |
| --- | --- |
| `murmur3_state<Width>` | The hash lanes, the number of bytes folded (`length`), the `seed` and the bytes of an unfinished block (`buffer`). A plain value: copy it, store it. The default is the empty message with seed 0; `murmur3_state<Width>{.seed = seed}` starts one with another seed. |
| `murmur3_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `murmur3_finalize(state)` | The hash of the message folded into `state`; the state is unchanged. |
| `murmur3_compute<Width>(data, seed = 0)` | `murmur3_finalize(murmur3_update(murmur3_state<Width>{.seed = seed}, data))`. |
| `murmur3_32_state`, `murmur3_128_state` | `murmur3_state<32>` and `murmur3_state<128>`. |
| `murmur3_32_compute(data, seed = 0)`, `murmur3_128_compute(data, seed = 0)` | `murmur3_compute<32>(data, seed)` and `murmur3_compute<128>(data, seed)`. |

`murmur3_state<Width>::value_type` is the hash: `std::uint32_t` for Width 32, and `hash128` for Width 128, the same
type as XXH3-128 returns. Its `low` half is h1 and its `high` half h2, so it is the 128-bit number the reference
implementation writes out least significant byte first. All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of
`std::byte`, `char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. At run time
`murmur3_compute` hashes contiguous ranges directly, without the state's buffer; other ranges are folded in 64-byte
chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never hashed: pass text as
`std::string_view`.

Every state value is valid, so nothing has preconditions. Before the first whole block (4 or 16 bytes), the lanes and
the unused bytes of the buffer are ignored.

## 3. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes; the empty message with seed 0 hashes to 0 (and to {0, 0} for
  Width 128).
- **Byte order:** blocks are read little-endian on every target, so a big-endian target gives the same hashes. The
  result is a number; serializing the 128-bit hash as the reference does means writing `low`, then `high`, each least
  significant byte first.
- **Long messages:** MurmurHash3_x86_32 mixes in the length modulo 2^32, MurmurHash3_x64_128 the 64-bit length. The
  reference implementation takes the length as an `int`, so it cannot hash messages of 2 GiB or more.

## 4. CPU acceleration

MurmurHash3 has no kernel, on any target. Every block updates the hash lanes through a rotation, a multiplication and
an addition of their previous values, and in MurmurHash3_x64_128 the second lane also takes the first lane of the same
block. Blocks cannot be processed in parallel, so vector instructions have nothing to work on. The block loop is
compiled once per width in `src/murmur3/block_loop.cpp`; MurmurHash3_x64_128 inputs under 256 bytes fold inline
instead, which saves the call, and the one-shot MurmurHash3_x86_32 runs a function there that keeps its lane in a
register.

MurmurHash3_x64_128 needs 64-bit multiplications, which 32-bit targets build from several 32-bit ones. On 32-bit
targets MurmurHash3_x86_32 is the faster of the two; on 64-bit targets MurmurHash3_x64_128 is at least as fast at 20
bytes and faster from 64 bytes.

## 5. Performance

### 5.1 x86-64: Core Ultra 7 155H

GCC, `-O2 -march=native`: MurmurHash3_x86_32 runs 5 850 MB/s at 20 bytes and 4 499 at 1 MiB, MurmurHash3_x64_128
6 140 and 10 006. Both are at least as fast as SMHasher's MurmurHash3.cpp with the same flags.

### 5.2 Cortex-A72: Raspberry Pi 4, 1.5 GHz

GCC 14.3, `-O2`, one core. MB/s (10⁶ bytes per second):

| Hash, mode | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| x86_32, AArch64 | 720 | 981 | 1 084 | 1 169 | 1 182 | 1 143 |
| x64_128, AArch64 | 646 | 1 081 | 1 430 | 1 634 | 1 672 | 1 619 |
| x86_32, AArch32 | 618 | 902 | 1 066 | 1 167 | 1 180 | 1 098 |
| x64_128, AArch32 | 278 | 408 | 536 | 595 | 604 | 599 |

On AArch64 MurmurHash3_x86_32 runs 0.95 to 1.0× as fast as SMHasher's MurmurHash3.cpp at every size, with `-O2` or
`-O2 -mcpu=cortex-a72`.

### 5.3 Cortex-M4: nRF52840 at 64 MHz, STM32L4A6 at 80 MHz

GCC 14.3, `-O2`, code in flash and data in RAM, measured with the cycle counter. At 4 KiB MurmurHash3_x86_32 takes
3.03 cycles per byte (26 MB/s at 80 MHz) and MurmurHash3_x64_128 4.43 (18 MB/s); at 20 bytes they reach 9.6 and
6.8 MB/s. Both chips run the same cycles per byte, so the figures scale with the clock.

## 6. Limitations

- **Not cryptographic:** MurmurHash3 does not resist inputs crafted to collide, even with a secret seed. Do not use it
  for message authentication or to protect hash tables from hostile keys.
- **Two variants only:** MurmurHash3_x86_128 is not provided.
- **Not an error check:** unlike a CRC, it guarantees the detection of no class of errors, burst errors included.
- No CPU acceleration (section 4). Code size has not been measured.
