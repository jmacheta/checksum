# MurmurHash3 user guide

The non-cryptographic hashes MurmurHash3_x86_32 and MurmurHash3_x64_128 with a 32-bit seed, at compile time or at run
time. One header, `checksum/murmur3.hpp`. For new designs prefer [xxHash](xxhash.md).

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

## 2. Definition

The hashes of the reference implementation, `MurmurHash3.cpp` of SMHasher: the values are equal on every target.
MurmurHash3_x86_128 is not provided.

## 3. API

`Width` is 32 for MurmurHash3_x86_32 and 128 for MurmurHash3_x64_128.

| Name | What it does |
| --- | --- |
| `murmur3_state<Width>` | The lanes, the byte count (`length`), the `seed` and an unfinished block (`buffer`). A plain value; the default is the empty message with seed 0, `murmur3_state<Width>{.seed = seed}` starts another. |
| `murmur3_update<Width, Isa>(state, data)` | Folds `data` into `state` and returns the new state. |
| `murmur3_finalize(state)` | The hash of the message folded into `state`. |
| `murmur3_compute<Width, Isa>(data, seed = 0)` | `murmur3_finalize(murmur3_update<Width, Isa>(murmur3_state<Width>{.seed = seed}, data))`. |
| `murmur3_32_state`, `murmur3_128_state`, `murmur3_32_compute<Isa>`, `murmur3_128_compute<Isa>` | Aliases for Width 32 and 128. |
| `murmur3_dispatch_engine<Width>` | `compute` and `update` with the preset chosen at run time. |

`value_type` is `std::uint32_t` for Width 32 and `hash128` (shared with XXH3-128) for Width 128, with h1 in `low` and
h2 in `high`. `Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr`
and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`,
`signed char` or `char8_t`; arrays of `char` are rejected so a literal's `'\0'` is never hashed (pass
`std::string_view`). `compute` hashes contiguous ranges directly. Every state value is valid, so nothing has
preconditions.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes; the empty message with seed 0 hashes to 0 (`{0, 0}` for Width 128).
- **Byte order:** blocks are read little-endian on every target. To serialize the 128-bit hash as the reference does,
  write `low`, then `high`, each least significant byte first.
- **Long messages:** x86_32 mixes in the length modulo 2^32, x64_128 the 64-bit length. The reference takes an `int`
  length, so it cannot hash 2 GiB or more.

## 5. CPU acceleration

MurmurHash3 has no kernel on any target: every block updates the lanes from their previous values, one serial chain. The
preset compiles the portable block loop for its instruction set ([Instruction-set
presets](../README.md#instruction-set-presets)); with MSVC (no target options) and on RISC-V (no target pragma), each
preset's copy is the same code as `isa::generic`. On 32-bit Arm with any preset but `thumb2_neon` and `thumb2_crc`,
x86_32 takes four blocks per iteration so in-order cores overlap the multiplications. Details:
[design/acceleration.md](design/acceleration.md#murmurhash3).

### 5.1 Choosing the preset at run time

A dispatch engine asks the CPU once, when it is made, and from then on runs the best variant this build has for it:

```cpp
#include <checksum/murmur3.hpp>

// Made once, during program start-up: detect_isa() queries the CPU or the operating system.
static checksum::murmur3_dispatch_engine<32> const murmur3_32;

std::uint32_t key_hash(std::string_view key) { return murmur3_32.compute(key, 42); }
```

`murmur3_32.preset()` names the chosen preset. An explicit preset chooses instead, e.g. from a configuration:
`checksum::murmur3_dispatch_engine<32>(checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time
where this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on
top of the variant. Detection does not run in constant expressions: there, call `murmur3_compute<32>` or give the engine
an explicit preset. Details and the list of engines: [Instruction-set presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#murmurhash3).

- **Pick the variant for the CPU:** x64_128 on 64-bit CPUs (at least as fast at 20 bytes, faster from 64); x86_32 on
  32-bit ones, where x64_128 builds 64-bit multiplications from 32-bit ones.
- **For new designs prefer xxHash** unless you need MurmurHash3 values: XXH3 is about 5× faster on x86-64 at 1 MiB,
  and XXH32 1.6× faster than x86_32 on a Cortex-M4.

## 7. Limitations

- **Not cryptographic:** MurmurHash3 does not resist inputs crafted to collide, even with a secret seed. Do not use it
  for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, it guarantees the detection of no class of errors.
- **Two variants only:** MurmurHash3_x86_128 is not provided.
