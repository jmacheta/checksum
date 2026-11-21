# XXH3

The seeded hashes XXH3-64 and XXH3-128, with values equal to the reference `xxhash.h` 0.8.4 on every target. They are
in `checksum/xxh3.hpp`, and every function is `constexpr` and `noexcept`. XXH32 and XXH64 have their own guide,
[xxhash.md](xxhash.md).

## The simplest way

```cpp
#include <checksum/xxh3.hpp>
using namespace std::literals;

static_assert(checksum::xxh3_compute<64>("abc"sv) == 0x78AF5F94892F3950);
static_assert(checksum::xxh3_compute<128>("abc"sv) ==
              checksum::hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});

std::uint64_t hash = checksum::xxh3_compute<64>(data);
checksum::hash128 seeded = checksum::xxh3_compute<128>(data, 1234);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## You know your target platform

When you build for a known CPU, name its preset as a template argument after the width. The program then runs that
preset's kernel directly, with no detection and no compiler flags. On x86-64, `isa::x86_avx2` reaches about
73 000 MB/s, 2.6× the SSE2 kernel of the default. Inputs up to 240 bytes run the same straight-line code with every
preset.

```cpp
std::uint64_t hash = checksum::xxh3_compute<64, checksum::isa::x86_avx2>(data, 1234);

// Data in pieces. The engine holds the seed; the state holds only the message.
constexpr checksum::xxh3_engine<64, checksum::isa::x86_avx2> xxh3(1234);
checksum::accumulator running{xxh3};
std::uint64_t value = running.update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build.
Make it once, as a `static` or global object.

```cpp
static checksum::xxh3_dispatch_engine<128> const xxh3(1234);
checksum::hash128 hash = xxh3.compute(data);
checksum::isa chosen = xxh3.preset();
```

## Reference

### API

`Width` is 64 or 128; the result is a `std::uint64_t` or a `hash128`, and the seed is a `std::uint64_t`.

| Name | Description |
| --- | --- |
| `xxh3_compute<Width, Isa>(data, seed = 0)` | The hash of `data`. |
| `xxh3_engine<Width, Isa>` | An engine holding the seed, `xxh3_engine<Width>{seed}`, with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)` and the static `preset()`. |
| `xxh3_state<Width>` | Eight accumulators, the byte count and a 256-byte buffer, 328 bytes in all. A plain value; `==` compares what the hash depends on. |
| `hash128` | `low` and `high` 64-bit halves, as in `XXH128_hash_t`, from `checksum/hash128.hpp`. |
| `xxh3_dispatch_engine<Width>` | The same members, with the constructor `(seed = 0, isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere and gives the same hash.
- Input is read little-endian on every target. The reference's canonical form writes the hash most significant byte
  first, and `high` before `low`.
- XXH3 uses the reference's default secret. A seed other than 0 derives a new 192-byte secret on the stack in each call
  that folds stripes.
- There is no `resume`, because a hash does not determine the state.

### Presets and kernels

| Preset | Kernel for inputs over 240 bytes |
| --- | --- |
| `x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | Two 256-bit vectors |
| `generic`, `x86_pclmul` on x86-64 | Four 128-bit SSE2 vectors |
| every preset on little-endian AArch64, `generic` included | Four lanes in NEON, four scalar |
| `thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | Eight lanes in NEON |
| `riscv_v` (VLEN ≥ 128) | One register group, tested in QEMU only |
| all others, Cortex-M and big-endian included | Portable loop |

### Performance

- Hash whole messages in one call when they are in memory, or stream pieces of 4 KiB or more. Each `update` copies the
  328-byte state, and with a seed it may derive the secret again.
- On 32-bit cores such as Cortex-M, [XXH32](xxhash.md) is about twice as fast.

Full figures are in [performance.md](performance.md#xxh3).

### Limitations

- XXH3 is not cryptographic: even with a secret seed it does not resist inputs crafted to collide. Do not use it for
  authentication or to protect hash tables from hostile keys.
- It is not an error check: unlike a CRC, it does not guarantee detecting any kind of error, not even a single flipped
  bit.
- Only the default secret is supported, so `XXH3_64bits_withSecret` and its relatives have no equivalent.
