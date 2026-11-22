# SHA-1 and SHA-2

The hashes of FIPS 180-4: SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, SHA-512/224 and SHA-512/256, with the digests of
OpenSSL on every target. They are in `checksum/sha.hpp`, and every function is `constexpr` and `noexcept`, so a digest
can be checked in a `static_assert` or stored next to firmware at build time.

## The simplest way

```cpp
#include <checksum/sha.hpp>
using namespace std::literals;
using checksum::sha_variant;

static_assert(checksum::sha_compute<sha_variant::sha256>("abc"sv)[0] == 0xBA);

std::array<std::uint8_t, 32> digest = checksum::sha_compute<sha_variant::sha256>(image);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed. The digest is a `std::array<std::uint8_t, N>` in the byte order of
the standard, the same bytes `sha256sum` prints:

```cpp
for(std::uint8_t byte : digest) {
  std::print("{:02x}", byte);
}
```

## Choosing a variant and a strategy for your CPU

On 64-bit CPUs SHA-512 is 1.3-1.6× faster than SHA-256, and SHA-512/256 gives a 32-byte digest at the speed of SHA-512.
On 32-bit cores SHA-256 is faster.

The second template argument trades code size for speed. `sha_compact`, the default, takes the least code and stack.
`sha_unrolled` runs SHA-1 about twice as fast everywhere, and SHA-256 1.2-1.5× faster on Arm, for up to 2.4× the code:

```cpp
using checksum::sha_unrolled;
auto digest = checksum::sha_compute<sha_variant::sha1, sha_unrolled>(object);

// Data in pieces, as a bootloader reads flash.
constexpr checksum::sha_engine<sha_variant::sha256, sha_unrolled> sha;
checksum::accumulator running{sha};
for(auto page : pages) {
  running.update(page);
}
bool valid = running.value() == expected;
```

When you know the CPU has SHA instructions, name the preset: the hash then runs them, whatever the strategy. On an
i7-13700K, `isa::x86_sha` (SHA-NI) runs SHA-256 at 2 600 MB/s, 5× the portable code, and short messages 3× faster.

```cpp
auto digest = checksum::sha_compute<sha_variant::sha256, checksum::sha_compact, checksum::isa::x86_sha>(image);
```

| Preset | Kernel |
| --- | --- |
| `x86_sha` (SHA-NI: Goldmont, Zen, Ice Lake and later) | SHA-1 and SHA-256; SHA-512 runs the portable code |
| `aarch64_sha2` (little-endian AArch64) | SHA-1 and SHA-256, tested in QEMU only |
| `aarch64_sha512` (little-endian AArch64; Apple M1, Neoverse V1/N2 and later) | As above, plus SHA-512, tested in QEMU only; not with MSVC |
| all others, the other x86 and AArch64 presets included | Portable code |

## You don't know the CPU, or you need broad compatibility

The dispatch engine picks the SHA instructions where the CPU has them, and the portable code elsewhere.

```cpp
static checksum::sha_dispatch_engine<sha_variant::sha256> const sha;
auto digest = sha.compute(data);
```

## Reference

### API

| Name | Description |
| --- | --- |
| `sha_variant` | `sha1`, `sha224`, `sha256`, `sha384`, `sha512`, `sha512_224`, `sha512_256`. |
| `sha_compact`, `sha_unrolled` | The strategies; `sha_strategy` is the concept that accepts them. |
| `sha_compute<Variant, Strategy, Isa>(data)` | The digest of `data`. |
| `sha_engine<Variant, Strategy, Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)` and the static `preset()`. |
| `sha_state<Variant>` | The hash value, the byte count and an unfinished block, the same for both strategies. A plain value; `==` compares what the digest depends on. |
| `sha_dispatch_engine<Variant, Strategy>` | The same members, with the constructor `(isa_set cpu = detect_isa())`. |

`Strategy` defaults to `sha_compact` and `Isa` to `isa::generic`. `accumulator` works with every engine.

- A message may be split anywhere and gives the same digest, up to 2^61 − 1 bytes.
- There is no `resume`, because a digest does not determine the state.
- The state is 96 bytes for SHA-1, 104 for SHA-224 and SHA-256 and 200 for the others, and it is passed by value.
- The message schedule takes 64 bytes of stack (128 for the SHA-512 family) with `sha_compact`, and 256-640 bytes with
  `sha_unrolled`.

### Performance

| At 4 KiB | `sha_compact` | `sha_unrolled` |
| --- | --- | --- |
| SHA-256, i7-13700K | 499 MB/s | 476 MB/s |
| SHA-256, Cortex-M4 at 80 MHz | 1.00 MB/s | 1.33 MB/s |
| SHA-256 code on Cortex-M4, `-O2` / `-Os` | 764 / 716 B | 1 188 / 978 B |

Full figures are in [performance.md](performance.md#sha-1-and-sha-2).

### Limitations

- SHA-1 is broken for collisions: use it only where a format requires it, such as Git object names.
- HMAC and other keyed constructions are not provided, and the code is not written to resist timing attacks on secret
  data: hash public data, such as firmware images and downloads. `==` on two digests is not constant-time either.
- SHA-3 and the bit-oriented messages of FIPS 180-4 (lengths that are not whole bytes) are not provided.
