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
register. On 32-bit Arm without NEON (Cortex-M, small in-order A-profile cores) MurmurHash3_x86_32 takes four blocks
per iteration: their multiplications do not depend on the lane, so an in-order core runs them while the earlier blocks
update it. That made it 1.66× faster at 4 KiB on a Cortex-M33 and 1.20× on a Cortex-M4; out-of-order cores already overlap the
blocks.

MurmurHash3_x64_128 needs 64-bit multiplications, which 32-bit targets build from several 32-bit ones. On 32-bit
targets MurmurHash3_x86_32 is the faster of the two; on 64-bit targets MurmurHash3_x64_128 is at least as fast at 20
bytes and faster from 64 bytes.

## 5. Performance guidelines

Measured figures are in [performance.md](performance.md#murmurhash3).

- **Pick the variant for the CPU:** MurmurHash3_x64_128 on 64-bit CPUs, where it is at least as fast at 20 bytes and
  faster from 64 bytes; MurmurHash3_x86_32 on 32-bit ones, where x64_128 builds its 64-bit multiplications from 32-bit
  ones.
- **For new designs, prefer xxHash** ([xxhash.md](xxhash.md)) unless you need MurmurHash3 values: XXH3 is about
  5× faster on x86-64 at 1 MiB, and XXH32 1.6× faster than MurmurHash3_x86_32 on a Cortex-M4.

## 6. Limitations

- **Not cryptographic:** MurmurHash3 does not resist inputs crafted to collide, even with a secret seed. Do not use it
  for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, it guarantees the detection of no class of errors, burst errors included.
- **Two variants only:** MurmurHash3_x86_128 is not provided.
