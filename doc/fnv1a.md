# FNV-1a

The hashes FNV-1a 32 and FNV-1a 64, with the values of the FNV reference on every target. They are in
`checksum/fnv1a.hpp`, and every function is `constexpr` and `noexcept`. FNV-1a suits short keys hashed at compile time
or in small hash tables; for long inputs, [XXH3](xxh3.md) is many times faster.

## The simplest way

```cpp
#include <checksum/fnv1a.hpp>
using namespace std::literals;

static_assert(checksum::fnv1a_compute<32>("foobar"sv) == 0xBF9CF968);

std::uint32_t hash = checksum::fnv1a_compute<32>(data);
std::uint64_t wide = checksum::fnv1a_compute<64>(name);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## Choosing a variant for your CPU

FNV-1a has no CPU kernels, because every byte depends on the previous one. Every preset runs the same portable loop,
so the choice that matters is the width. Both run at the same speed on x86-64. On 32-bit cores FNV-1a 32 is faster,
and on a Cortex-A72 in AArch64 too.

```cpp
// Data in pieces. A stored hash continues where it ended.
constexpr checksum::fnv1a_engine<64> fnv1a;
checksum::accumulator running{fnv1a};
std::uint64_t value = running.update(prefix).update(key).value();
std::uint64_t same = fnv1a.finalize(fnv1a.update(fnv1a.resume(fnv1a.compute(prefix)), key));
```

## You don't know the CPU, or you need broad compatibility

You need nothing extra: the portable loop is the fastest code on every CPU. The dispatch engine exists only so that
code that dispatches all its algorithms can treat FNV-1a alike; it always runs the `generic` loop.

```cpp
static checksum::fnv1a_dispatch_engine<32> const fnv1a;
std::uint32_t hash = fnv1a.compute(data);
```

## Reference

### API

`Width` is 32 or 64, and the result is a `std::uint32_t` or a `std::uint64_t`.

| Name | Description |
| --- | --- |
| `fnv1a_compute<Width, Isa>(data)` | The hash of `data`. |
| `fnv1a_engine<Width, Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `resume(hash)`, `compute(data)` and the static `preset()`. |
| `fnv1a_state<Width>` | The running hash, starting at the offset basis. A plain value with `==`. |
| `fnv1a_dispatch_engine<Width>` | The same members, with the constructor `(isa_set cpu = detect_isa())`. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere and gives the same hash. The empty message hashes to the offset basis, 0x811C9DC5
  and 0xCBF29CE484222325.
- `resume(hash)` continues from any stored hash, because the hash is the whole state.

### Performance

- At 4 KiB, FNV-1a 32 runs about 1 400 MB/s on an i7-13700K and 18 MB/s on an 80 MHz Cortex-M4.
- The inline loop takes 132 bytes of code on a Cortex-M4 at `-O2`.

Full figures are in [performance.md](performance.md#fnv-1a).

### Limitations

- FNV-1a is not cryptographic and has no seed: anyone can craft keys that collide. Do not use it for authentication or
  to protect hash tables from hostile keys.
- It is not an error check: unlike a CRC, it does not guarantee detecting any kind of error.
- FNV-1 and the 128-bit and wider variants are not provided.
