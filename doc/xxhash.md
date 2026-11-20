# xxHash user guide

The non-cryptographic hashes XXH32 and XXH64 with a seed, at compile time or at run time. One header,
`checksum/xxhash.hpp`. For XXH3-64 and XXH3-128, see [xxh3.md](xxh3.md).

## 1. Quick start

```cpp
#include <checksum/xxhash.hpp>
using namespace std::literals;

// One call, seed 0 or another seed.
std::uint32_t small = checksum::xxhash_compute<32>(key);
std::uint64_t hash = checksum::xxhash_compute<64>(key, 1234);

// Incremental: the engine holds the seed, the state only the message; a message may be split anywhere.
constexpr checksum::xxhash_engine<64> xxh64(1234);
auto state = xxh64.update(xxh64.initial(), first_part);
state = xxh64.update(state, second_part);
std::uint64_t value = xxh64.finalize(state);

// Compile time.
static_assert(checksum::xxhash_compute<32>("abc"sv) == 0x32D153FF);
static_assert(checksum::xxhash_compute<64>("abc"sv) == 0x44BC2CF5AD770999);
```

`examples/xxhash` (message IDs at compile time, the XXH64 of a file, a cache keyed by a hash) has complete programs.

## 2. Definition

The hashes of the reference implementation, `xxhash.h` 0.8.4: the values are equal on every target.

## 3. API

The family has an engine, a state and one `compute` function. The engine holds the seed. The engine's members are
`initial()` (the empty message), `update(state, data)` (folds `data` into `state` and returns the new state),
`finalize(state)` (the hash), `compute(data)` (`finalize(update(initial(), data))`) and the static `preset()`; the hash
has no `resume`, since its result does not determine the state. `Isa` is an instruction-set preset, `isa::generic` by
default (section 5). All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any
range of `std::byte`, `char`, `unsigned char`, `signed char` or `char8_t`; arrays of `char` are rejected so a literal's
`'\0'` is never hashed (pass `std::string_view`). `compute` hashes contiguous ranges directly, without the state's
buffer. Every state value is valid, so nothing has preconditions; a state is a plain value, so one prefix can be hashed
several ways.

`Width` is 32 for XXH32 and 64 for XXH64.

| Name | What it does |
| --- | --- |
| `xxhash_engine<Width, Isa>` | The engine: `xxhash_engine<Width, Isa>{seed}`, seed 0 by default. The seed is a `std::uint32_t` for XXH32 and a `std::uint64_t` for XXH64. |
| `xxhash_state<Width>` | Four lanes, the byte count (`length`) and an unfinished stripe (`buffer`, 16 or 32 bytes). The default is the empty message. `value_type` is `std::uint32_t` or `std::uint64_t`. |
| `xxhash_compute<Width, Isa>(data, seed = 0)` | `xxhash_engine<Width, Isa>{seed}.compute(data)`. |
| `xxhash_dispatch_engine<Width>` | The same members as `xxhash_engine<Width>`, made with `(seed = 0, cpu = detect_isa())`, with the preset chosen at run time. |

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes.
- **Byte order:** input words are read little-endian on every target. The result is a number; the reference's canonical
  form writes most significant byte first.
- **Long messages:** the length is a 64-bit count; XXH32 mixes it in modulo 2^32, as the reference does.

## 5. CPU acceleration

XXH32 and XXH64 have no kernel on any target: each lane is a serial multiply-rotate chain, and the scalar loop already
runs the four lanes side by side. Their preset compiles the portable loop for its instruction set; with MSVC (no
target options) and on RISC-V (no target pragma), each preset's copy is the same code as `isa::generic`. Presets are
explained in [Instruction-set presets](../README.md#instruction-set-presets).

### 5.1 Choosing the preset at run time

A dispatch engine asks the CPU once, when it is made, and from then on runs the best variant this build has for it:

```cpp
#include <checksum/xxhash.hpp>

// Made once, during program start-up: detect_isa() queries the CPU or the operating system.
static checksum::xxhash_dispatch_engine<64> const xxh64;

std::uint64_t key_hash(std::string_view key) { return xxh64.compute(key); }
```

`xxh64.preset()` names the chosen preset. An explicit preset chooses instead, e.g. from a configuration:
`checksum::xxhash_dispatch_engine<64>(0, checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time where
this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on top of
the variant. Detection does not run in constant expressions: there, call `xxhash_compute<32>` / `xxhash_compute<64>` or
give the engine an explicit preset. Details and the list of engines: [Instruction-set
presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#xxhash).

- **Pick the hash for the CPU.** On 32-bit cores (Cortex-M, AArch32) XXH32 is about twice the others from 256 bytes on
  a Cortex-M4; XXH64 is slow there, since it needs 64-bit multiplications. On 64-bit CPUs XXH3 is fastest; see
  [xxh3.md](xxh3.md).
- **Hash whole messages in one call** when they are in memory, or stream in pieces of 4 KiB or more.

## 7. Limitations

- **Not cryptographic:** XXH32 and XXH64 do not resist inputs crafted to collide, even with a secret seed. Do not use
  them for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, they guarantee the detection of no class of errors.
