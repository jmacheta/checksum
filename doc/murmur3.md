# MurmurHash3 user guide

The non-cryptographic hashes MurmurHash3_x86_32 and MurmurHash3_x64_128 with a 32-bit seed, at compile time or at run
time. One header, `checksum/murmur3.hpp`. For new designs prefer [XXH3](xxh3.md).

## 1. Quick start

The three ways to compute, the same in every guide: in a constant expression, with an instruction-set preset named in
the program (section 5), and with the preset chosen for the running CPU (section 5.1). `data`, `header` and `payload`
are spans or other ranges of bytes.

```cpp
#include <checksum/murmur3.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::murmur3_compute<32>("Hello, world!"sv, 1234) == 0xFAF6CDB3);

// Explicit preset and seed: AVX2 where this build has it (x86-64), isa::generic elsewhere.
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  std::uint32_t hash = checksum::murmur3_compute<32, avx2>(data, 1234);
  constexpr checksum::murmur3_engine<128, avx2> murmur3(1234);  // the engine holds the seed, the state only the message
  checksum::hash128 value = murmur3.finalize(murmur3.update(murmur3.update(murmur3.initial(), header), payload));
}

// Run-time dispatch: the same interface as every algorithm; MurmurHash3 runs isa::generic on every CPU.
static checksum::murmur3_dispatch_engine<128> const murmur3(1234);
checksum::isa chosen = murmur3.preset();
checksum::accumulator running{murmur3};
checksum::hash128 value = running.update(header).update(payload).value();
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
| `murmur3_engine<Width, Isa>` | The engine, `murmur3_engine<Width, Isa>{seed}` with a `std::uint32_t` seed, 0 by default. `initial()` is the empty message, `update(state, data)` folds `data` into `state` and returns the new state, `finalize(state)` is the hash, `compute(data)` is `finalize(update(initial(), data))`, and the static `preset()` is `Isa`. There is no `resume`: the hash does not determine the state. |
| `murmur3_state<Width>` | The lanes, the byte count (`length`) and an unfinished block (`buffer`). A plain value; the default is the empty message. `==` compares what the hash depends on, so one message split differently gives equal states. |
| `murmur3_compute<Width, Isa>(data, seed = 0)` | `murmur3_engine<Width, Isa>{seed}.compute(data)`. |
| `murmur3_dispatch_engine<Width>` | The same members as `murmur3_engine<Width>`, made with `(seed = 0, cpu = detect_isa())`; `preset()`, not static, is always `generic` (section 5.1). |
| `accumulator<Engine>` | `checksum::accumulator running{engine}` (`checksum/engine.hpp`, shared by every algorithm): a pointer to the engine and a state, with `update(data)` (chainable), `value()`, `state()` and `reset()`. The engine must outlive it. |

`value_type` is `std::uint32_t` for Width 32 and `hash128` (shared with XXH3-128) for Width 128, with h1 in `low` and h2
in `high`. `Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and
`noexcept`. `data` is a `std::span<std::byte const>` or any `byte_range`: an input range of non-`volatile` `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`; non-contiguous ranges are copied through a 64-byte stack buffer.
Arrays of `char` and `char8_t` are rejected so a string literal's `'\0'` is never included: pass `std::string_view`.
`compute` hashes contiguous ranges directly. Every state value is valid, so nothing has preconditions.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes; the empty message with seed 0 hashes to 0 (`{0, 0}` for Width 128).
- **Byte order:** blocks are read little-endian on every target. To serialize the 128-bit hash as the reference does,
  write `low`, then `high`, each least significant byte first.
- **Long messages:** x86_32 mixes in the length modulo 2^32, x64_128 the 64-bit length. The reference takes an `int`
  length, so it cannot hash 2 GiB or more.

## 5. CPU acceleration

MurmurHash3 has no kernel on any target: every block updates the lanes from their previous values, one serial chain.
Every preset ([Instruction-set presets](../README.md#instruction-set-presets)) runs the one block loop of
`isa::generic`: copies compiled per preset were the same code or no faster. On 32-bit Arm x86_32 takes four blocks per
iteration so in-order cores overlap the multiplications. Details:
[design/acceleration.md](design/acceleration.md#murmurhash3).

### 5.1 Choosing the preset at run time

The dispatch engine of section 1 has the members of every dispatch engine, so code that dispatches all its algorithms
treats MurmurHash3 alike, but it always runs the `isa::generic` loop and `preset()` is `generic`, on every architecture
and compiler; the `cpu` argument changes nothing and a call costs nothing on top of the static engine. Detection does
not run in constant expressions: there, call `murmur3_compute<32>` or give the engine an explicit set. Details and the
list of engines: [Instruction-set presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#murmurhash3).

- **Pick the variant for the CPU:** x64_128 on 64-bit CPUs (faster from 64 bytes; at 20 bytes 5 % faster on x86-64,
  10 % slower on a Cortex-A72); x86_32 on 32-bit ones, where x64_128 builds 64-bit multiplications from 32-bit ones.
- **For new designs prefer xxHash** unless you need MurmurHash3 values: XXH3 is about 5× faster on x86-64 at 1 MiB,
  and XXH32 1.6× faster than x86_32 on a Cortex-M4.

## 7. Limitations

- **Not cryptographic:** MurmurHash3 does not resist inputs crafted to collide, even with a secret seed. Do not use it
  for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, it guarantees the detection of no class of errors.
- **Two variants only:** MurmurHash3_x86_128 is not provided.
