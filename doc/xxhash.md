# XXH32 and XXH64

The seeded hashes XXH32 and XXH64, with values equal to the reference `xxhash.h` 0.8.4 on every target. They are in
`checksum/xxhash.hpp`, and every function is `constexpr` and `noexcept`. XXH3 has its own guide, [xxh3.md](xxh3.md).

## The simplest way

```cpp
#include <checksum/xxhash.hpp>
using namespace std::literals;

static_assert(checksum::xxhash_compute<32>("abc"sv) == 0x32D153FF);
static_assert(checksum::xxhash_compute<64>("abc"sv) == 0x44BC2CF5AD770999);

std::uint64_t hash = checksum::xxhash_compute<64>(data);
std::uint64_t seeded = checksum::xxhash_compute<64>(data, 1234);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## Choosing a variant for your CPU

XXH32 and XXH64 have no CPU kernels, because each lane is a serial multiply-rotate chain that vector units cannot speed
up. Every preset runs the same portable loop, so the choice that matters is the width. On 32-bit cores, XXH32 is about
twice as fast as the alternatives, while XXH64 is slow there because it needs 64-bit multiplications. On 64-bit CPUs,
[XXH3](xxh3.md) is faster than both.

```cpp
// Data in pieces. The engine holds the seed; the state holds only the message.
constexpr checksum::xxhash_engine<32> xxh32(1234);
checksum::accumulator running{xxh32};
std::uint32_t value = running.update(header).update(payload).value();
```

## You don't know the CPU, or you need broad compatibility

You need nothing extra: the portable loop is the fastest code on every CPU. The dispatch engine exists only so that
code that dispatches all its algorithms can treat xxHash alike; it always runs the `generic` loop.

```cpp
static checksum::xxhash_dispatch_engine<64> const xxh64(1234);
std::uint64_t hash = xxh64.compute(data);
```

## Reference

### API

`Width` is 32 or 64; the result and the seed are a `std::uint32_t` or a `std::uint64_t`.

| Name | Description |
| --- | --- |
| `xxhash_compute<Width, Isa>(data, seed = 0)` | The hash of `data`. |
| `xxhash_engine<Width, Isa>` | An engine holding the seed, `xxhash_engine<Width>{seed}`, with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)` and the static `preset()`. |
| `xxhash_state<Width>` | Four lanes, the byte count and an unfinished stripe. A plain value; `==` compares what the hash depends on. |
| `xxhash_dispatch_engine<Width>` | The same members, with the constructor `(seed = 0, isa_set cpu = detect_isa())`. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere and gives the same hash.
- Input is read little-endian on every target. The reference's canonical form writes the hash most significant byte
  first.
- There is no `resume`, because a hash does not determine the state.

### Performance

- XXH32 runs about 50 MB/s on an 80 MHz Cortex-M4 and 100 MB/s on a 128 MHz Cortex-M33, at 4 KiB.
- Hash whole messages in one call when they are in memory, or stream pieces of 4 KiB or more.

Full figures, including the measurements behind dropping per-preset loops, are in
[performance.md](performance.md#xxhash).

### Limitations

- The hashes are not cryptographic: even with a secret seed they do not resist inputs crafted to collide. Do not use
  them for authentication or to protect hash tables from hostile keys.
- They are not error checks: unlike a CRC, they do not guarantee detecting any kind of error, not even a single flipped
  bit.
