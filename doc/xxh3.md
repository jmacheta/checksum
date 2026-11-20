# XXH3 user guide

The non-cryptographic hashes XXH3-64 and XXH3-128 with a seed, at compile time or at run time. One header,
`checksum/xxh3.hpp`. XXH32 and XXH64 are in [xxhash.md](xxhash.md). The `hash128` type is shared with MurmurHash3.

## 1. Quick start

The three ways to compute, the same in every guide: in a constant expression, with an instruction-set preset named in
the program (section 5), and with the preset chosen for the running CPU (section 5.1). `data`, `header` and `payload`
are spans or other ranges of bytes.

```cpp
#include <checksum/xxh3.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::xxh3_compute<64>("abc"sv) == 0x78AF5F94892F3950);
static_assert(checksum::xxh3_compute<128>("abc"sv) ==
              checksum::hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});

// Explicit preset and seed: AVX2 where this build has it (x86-64), isa::generic elsewhere.
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  std::uint64_t hash = checksum::xxh3_compute<64, avx2>(data, 1234);
  constexpr checksum::xxh3_engine<64, avx2> xxh3(1234);  // the engine holds the seed, the state only the message
  std::uint64_t value = xxh3.finalize(xxh3.update(xxh3.update(xxh3.initial(), header), payload));
}

// Run-time dispatch: the best preset of this build for the running CPU, chosen once.
static checksum::xxh3_dispatch_engine<128> const xxh3(1234);
checksum::isa chosen = xxh3.preset();
checksum::accumulator running{xxh3};
checksum::hash128 value = running.update(header).update(payload).value();
```

`examples/xxh3` has complete programs: asset IDs at compile time, a file read in chunks, deduplication by XXH3-128, and
a seeded hash table.

## 2. Definition

The hashes of the reference implementation, `xxhash.h` 0.8.4: the values are equal on every target. XXH3 always uses
the reference's 192-byte default secret, kept in read-only data; a seed other than 0 derives a secret from it.

## 3. API

`Width` is 64 for XXH3-64 and 128 for XXH3-128.

| Name | What it does |
| --- | --- |
| `xxh3_engine<Width, Isa>` | The engine, `xxh3_engine<Width, Isa>{seed}`, seed 0 by default. `initial()` is the empty message, `update(state, data)` folds `data` into `state` and returns the new state, `finalize(state)` is the hash, `compute(data)` is `finalize(update(initial(), data))`, and the static `preset()` is `Isa`. There is no `resume`: the hash does not determine the state. The seed is a `std::uint64_t`; `value_type`, the hash, is `std::uint64_t` or `hash128`. `finalize` runs the stripe loop of `Isa` for the remaining buffer. |
| `xxh3_state<Width>` | Eight accumulators, `length` and a 256-byte `buffer`: 328 bytes. A plain value; the default is the empty message. `==` compares what the hash depends on, so one message split differently gives equal states. |
| `xxh3_compute<Width, Isa>(data, seed = 0)` | `xxh3_engine<Width, Isa>{seed}.compute(data)`. |
| `hash128` | `low` and `high` 64 bits, as `XXH128_hash_t`; compares with `==`. MurmurHash3_x64_128 returns it too. |
| `xxh3_dispatch_engine<Width>` | The same members as `xxh3_engine<Width>`, made with `(seed = 0, cpu = detect_isa())`; `preset()`, not static, names the preset chosen at run time (section 5.1). |
| `accumulator<Engine>` | `checksum::accumulator running{engine}` (`checksum/engine.hpp`, shared by every algorithm): a pointer to the engine and a state, with `update(data)` (chainable), `value()`, `state()` and `reset()`. The engine must outlive it. |

`Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and `noexcept`.
`data` is a `std::span<std::byte const>` or any `byte_range`: an input range of non-`volatile` `std::byte`, `char`,
`unsigned char`, `signed char` or `char8_t`; non-contiguous ranges are copied through a 64-byte stack buffer. Arrays of
`char` and `char8_t` are rejected so a string literal's `'\0'` is never included: pass `std::string_view`. `compute`
hashes contiguous ranges directly, without the state's buffer. Every state value is valid, so nothing has preconditions.

The state keeps the last 1 to 256 bytes in its buffer, so it can hash the end of the message as the one-shot function
does; messages up to 240 bytes are hashed from the buffer alone.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes. XXH3 with a seed other than 0 derives a 192-byte secret on the
  stack in each call that folds stripes (more than 240 bytes in one shot, an update that folds its buffer).
- **Byte order:** input words are read little-endian on every target. The result is a number or a pair of numbers;
  the reference's canonical form writes most significant byte first, and `high` before `low`.
- **Long messages:** the length is a 64-bit count.
- **Streaming cost:** `xxh3_engine::update` copies its 328-byte state in and out per call, which weighs on short
  pieces. The `byte_range` overload folds all its chunks into one state.

## 5. CPU acceleration

XXH3 runs straight-line code up to 240 bytes, the same in every build. Longer inputs fold 64-byte stripes into eight
accumulators, which a kernel keeps in vector registers. Presets are explained in
[Instruction-set presets](../README.md#instruction-set-presets).

| Preset | XXH3 kernel |
| --- | --- |
| `isa::x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | Two 256-bit vectors |
| `isa::generic`, `x86_pclmul` on x86-64 | Four 128-bit vectors |
| every preset of little-endian AArch64 (GCC, Clang, MSVC, clang-cl), `isa::generic` included | Four lanes in NEON, four scalar |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm (GCC, Clang) | All eight lanes in NEON |
| `isa::riscv_v` (VLEN ≥ 128) | One register group; tested in QEMU only |
| everything else: `isa::generic` elsewhere, `thumb2`, `thumb2_dsp`, `riscv_zbc`, big-endian targets | Portable loop |

Decisions per preset: [design/acceleration.md](design/acceleration.md#xxh3).

### 5.1 Choosing the preset at run time

The dispatch engine of section 1 asks the CPU once, when it is made, and from then on runs the best variant this build
has for it. `preset()` names the chosen preset; on RISC-V only `riscv_v`, the preset with a kernel, counts, so a CPU with Zbc alone runs `generic`. An explicit preset chooses instead, e.g. from a configuration:
`checksum::xxh3_dispatch_engine<64>(0, checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time where
this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on top of
the variant. Detection does not run in constant expressions: there, call `xxh3_compute<64>` / `xxh3_compute<128>` or
give the engine an explicit preset. Details and the list of engines: [Instruction-set
presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#xxh3).

- **Pick the hash for the CPU.** On 64-bit CPUs XXH3 is the fastest: about 48 000 MB/s with `isa::x86_avx2`, twice the
  SSE2 kernel of `isa::generic`. On 32-bit cores (Cortex-M, AArch32) XXH32 is about twice the others from 256 bytes on
  a Cortex-M4; XXH64 is slow there, since it needs 64-bit multiplications. See [xxhash.md](xxhash.md) for XXH32 and
  XXH64.
- **Hash whole messages in one call** when they are in memory, or stream in pieces of 4 KiB or more: each `update`
  copies the state, and one that folds stripes with a seed other than 0 derives the secret again (section 4).

## 7. Limitations

- **Not cryptographic:** XXH3 does not resist inputs crafted to collide, even with a secret seed. Do not use it for
  message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, XXH3 guarantees the detection of no class of errors.
- **Default secret only:** no custom secret, so `XXH3_64bits_withSecret` and related variants are not provided.
