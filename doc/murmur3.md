# MurmurHash3

The seeded hashes MurmurHash3_x86_32 and MurmurHash3_x64_128, with values equal to SMHasher's `MurmurHash3.cpp` on every
target. They are in `checksum/murmur3.hpp`, and every function is `constexpr` and `noexcept`. Use them where you need
MurmurHash3 values; for new designs, [XXH3](xxh3.md) is faster: on x86-64 at 1 MiB, about 2× with
`isa::generic` and 5× with `isa::x86_avx2`.

## The simplest way

```cpp
#include <checksum/murmur3.hpp>
using namespace std::literals;

static_assert(checksum::murmur3_compute<32>("Hello, world!"sv, 1234) == 0xFAF6CDB3);

std::uint32_t hash = checksum::murmur3_compute<32>(data);
checksum::hash128 wide = checksum::murmur3_compute<128>(data, 1234);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## Choosing a variant for your CPU

MurmurHash3 has no CPU kernels, because every block depends on the previous one. Every preset runs the same portable
loop, so the choice that matters is the variant. Use x64_128 on 64-bit CPUs, where it is faster from 64 bytes, and
x86_32 on 32-bit ones, where x64_128 must build its 64-bit multiplications from 32-bit ones.

```cpp
// Data in pieces. The engine holds the seed; the state holds only the message.
constexpr checksum::murmur3_engine<128> murmur3(1234);
checksum::accumulator running{murmur3};
checksum::hash128 value = running.update(header).update(payload).value();
```

## You don't know the CPU, or you need broad compatibility

You need nothing extra: the portable loop is the fastest code on every CPU. The dispatch engine exists only so that
code that dispatches all its algorithms can treat MurmurHash3 alike; it always runs the `generic` loop.

```cpp
static checksum::murmur3_dispatch_engine<128> const murmur3(1234);
checksum::hash128 hash = murmur3.compute(data);
```

## Reference

### API

`Width` is 32 for x86_32 and 128 for x64_128. The result is a `std::uint32_t` or a `hash128`, and the seed is always a
`std::uint32_t`.

| Name | Description |
| --- | --- |
| `murmur3_compute<Width, Isa>(data, seed = 0)` | The hash of `data`. |
| `murmur3_engine<Width, Isa>` | An engine holding the seed, `murmur3_engine<Width>{seed}`, with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)` and the static `preset()`. |
| `murmur3_state<Width>` | The lanes, the byte count and an unfinished block. A plain value; `==` compares what the hash depends on. |
| `hash128` | h1 in `low` and h2 in `high`, from `checksum/hash128.hpp`. |
| `murmur3_dispatch_engine<Width>` | The same members, with the constructor `(seed = 0, isa_set cpu = detect_isa())`. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere and gives the same hash. The empty message with seed 0 hashes to 0.
- Blocks are read little-endian on every target. To serialize the 128-bit hash as the reference does, write `low` and
  then `high`, each least significant byte first.
- The reference takes an `int` length and stops at 2 GiB; this one has no such limit.
- There is no `resume`, because a hash does not determine the state.

### Performance

- At 4 KiB, x86_32 runs about 32 MB/s on an 80 MHz Cortex-M4, and XXH32 1.6× that.
- Against SMHasher's code, both variants are at least as fast on x86-64. On a Cortex-A72, x86_32 runs 0.95-1.0× in
  AArch64 and 0.71-0.97× in AArch32, slowest on 20-byte inputs.

Full figures are in [performance.md](performance.md#murmurhash3).

### Limitations

- MurmurHash3 is not cryptographic: even with a secret seed it does not resist inputs crafted to collide. Do not use it
  for authentication or to protect hash tables from hostile keys.
- It is not an error check: unlike a CRC, it does not guarantee detecting any kind of error, not even a single flipped
  bit.
- MurmurHash3_x86_128 is not provided.
