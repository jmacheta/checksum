# xxHash user guide

The non-cryptographic hashes XXH32, XXH64, XXH3-64 and XXH3-128 with a seed, at compile time or at run time. XXH32 and
XXH64 are in `checksum/xxhash.hpp`, XXH3-64 and XXH3-128 in `checksum/xxh3.hpp`.

## 1. Quick start

```cpp
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>
using namespace std::literals;

// One call, seed 0 or another seed.
std::uint32_t small = checksum::xxh32_compute(key);
std::uint64_t hash = checksum::xxh3_64_compute(key, 1234);
checksum::hash128 wide = checksum::xxh3_128_compute(key);

// Incremental: a message may be split anywhere.
checksum::xxh3_64_state state{.seed = 1234};
state = checksum::xxh3_update(state, first_part);
state = checksum::xxh3_update(state, second_part);
std::uint64_t value = checksum::xxh3_finalize(state);

// Compile time.
static_assert(checksum::xxh32_compute("abc"sv) == 0x32D153FF);
static_assert(checksum::xxh64_compute("abc"sv) == 0x44BC2CF5AD770999);
static_assert(checksum::xxh3_64_compute("abc"sv) == 0x78AF5F94892F3950);
static_assert(checksum::xxh3_128_compute("abc"sv) ==
              checksum::hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});
```

`examples/xxh3` (asset IDs at compile time, a file read in chunks, deduplication by XXH3-128, a seeded hash table) and
`examples/xxhash` (message IDs at compile time, the XXH64 of a file, a cache keyed by a hash) have complete programs.

## 2. Definition

The hashes of the reference implementation, `xxhash.h` 0.8.4: the values are equal on every target. XXH3 always uses
the reference's 192-byte default secret, kept in read-only data; a seed other than 0 derives a secret from it.

## 3. API

Both families have a state, `update`, `finalize` and `compute` with their own prefix. `Isa` is an instruction-set
preset, `isa::generic` by default (section 5). All functions are `constexpr` and `noexcept`. `data` is a
`std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`, `signed char` or `char8_t`; arrays
of `char` are rejected so a literal's `'\0'` is never hashed (pass `std::string_view`). `compute` hashes contiguous
ranges directly, without the state's buffer. Every state value is valid, so nothing has preconditions; a state is a
plain value, so one prefix can be hashed several ways.

### 3.1 XXH32 and XXH64

`Width` is 32 for XXH32 and 64 for XXH64.

| Name | What it does |
| --- | --- |
| `xxhash_state<Width>` | Four lanes, the byte count (`length`), the `seed` and an unfinished stripe (`buffer`, 16 or 32 bytes). The default is the empty message with seed 0; `xxhash_state<Width>{.seed = seed}` starts another. `value_type` is `std::uint32_t` or `std::uint64_t`. |
| `xxhash_update<Width, Isa>(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxhash_finalize(state)` | The hash of the message folded into `state`. |
| `xxhash_compute<Width, Isa>(data, seed = 0)` | `xxhash_finalize(xxhash_update<Width, Isa>(xxhash_state<Width>{.seed = seed}, data))`. The seed is a `std::uint32_t` for XXH32 and a `std::uint64_t` for XXH64; XXH3 takes a `std::uint64_t`. |
| `xxh32_state`, `xxh64_state`, `xxh32_compute<Isa>`, `xxh64_compute<Isa>` | Aliases for Width 32 and 64. |
| `xxhash_dispatch_engine<Width>` | `compute` and `update` with the preset chosen at run time. |

### 3.2 XXH3-64 and XXH3-128

`Width` is 64 for XXH3-64 and 128 for XXH3-128.

| Name | What it does |
| --- | --- |
| `xxh3_state<Width>` | Eight accumulators, `length`, `seed` and a 256-byte `buffer`: 336 bytes. The default is the empty message with seed 0; `xxh3_state<Width>{.seed = seed}` starts another. `value_type` is `std::uint64_t` or `hash128`. |
| `xxh3_update<Width, Isa>(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxh3_finalize<Width, Isa>(state)` | The hash of the message folded into `state`; `Isa` runs the stripe loop for the remaining buffer. |
| `xxh3_compute<Width, Isa>(data, seed = 0)` | `xxh3_finalize<Width, Isa>(xxh3_update<Width, Isa>(xxh3_state<Width>{.seed = seed}, data))`. |
| `xxh3_64_state`, `xxh3_128_state`, `xxh3_64_compute<Isa>`, `xxh3_128_compute<Isa>` | Aliases for Width 64 and 128. |
| `hash128` | `low` and `high` 64 bits, as `XXH128_hash_t`; compares with `==`. MurmurHash3_x64_128 returns it too. |
| `xxh3_dispatch_engine<Width>` | `compute`, `update` and `finalize` with the preset chosen at run time. |

The state keeps the last 1 to 256 bytes in its buffer, so it can hash the end of the message as the one-shot function
does; messages up to 240 bytes are hashed from the buffer alone.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes. XXH3 with a seed other than 0 derives a 192-byte secret on the
  stack in each call that folds stripes (more than 240 bytes in one shot, an update that folds its buffer).
- **Byte order:** input words are read little-endian on every target. The result is a number or a pair of numbers;
  the reference's canonical form writes most significant byte first, and `high` before `low`.
- **Long messages:** the length is a 64-bit count; XXH32 mixes it in modulo 2^32, as the reference does.
- **Streaming cost:** `xxh3_update` copies its 336-byte state in and out per call. In 64-byte pieces XXH3 streams about
  4× slower than one call; from 4 KiB pieces the copy is negligible. The `byte_range` overload folds all its chunks into
  one state.

## 5. CPU acceleration

XXH32 and XXH64 have no kernel on any target: each lane is a serial multiply-rotate chain, and the scalar loop already
runs the four lanes side by side. Their preset compiles the portable loop for its instruction set.

XXH3 runs straight-line code up to 240 bytes, the same in every build. Longer inputs fold 64-byte stripes into eight
accumulators, which a kernel keeps in vector registers. Presets are explained in
[Instruction-set presets](../README.md#instruction-set-presets).

| Preset | XXH3 kernel |
| --- | --- |
| `isa::x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | Two 256-bit vectors |
| `isa::generic`, `x86_pclmul` on x86-64 | Four 128-bit vectors |
| every preset of little-endian AArch64 with GCC or Clang, `isa::generic` included | Four lanes in NEON, four scalar |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | All eight lanes in NEON |
| `isa::riscv_v` (VLEN ≥ 128) | One register group; tested in QEMU only |
| everything else: `isa::thumb2`, `thumb2_dsp`, `riscv_zbc`, big-endian targets, MSVC ARM64 | Portable loop |

Decisions per preset: [design/acceleration.md](design/acceleration.md#xxh3).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#xxhash).

- **Pick the hash for the CPU.** On 64-bit CPUs XXH3 is the fastest: about 48 000 MB/s with `isa::x86_avx2`, twice the
  SSE2 kernel of `isa::generic`. On 32-bit cores (Cortex-M, AArch32) XXH32 is, about twice the others from 256 bytes on
  a Cortex-M4; XXH64 is slow there, since it needs 64-bit multiplications.
- **Hash whole messages in one call** when they are in memory, or stream in pieces of 4 KiB or more. A seed other than
  0 costs about 25 % in 256-byte pieces and 6 % in 4 KiB pieces.

## 7. Limitations

- **Not cryptographic:** none of these hashes resists inputs crafted to collide, even with a secret seed. Do not use
  them for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, they guarantee the detection of no class of errors.
- **Default secret only:** no custom secret, so `XXH3_64bits_withSecret` and related variants are not provided.
