# SHA-1 and SHA-2

The hashes of FIPS 180-4: SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, SHA-512/224 and SHA-512/256, with the digests of
OpenSSL on every target. They are in `checksum/sha.hpp`, and every function is `constexpr` and `noexcept`, so a digest
can be checked in a `static_assert` or stored next to firmware at build time.

## The simplest way

```cpp
#include <checksum/sha.hpp>
using namespace std::literals;
using enum checksum::sha_variant;

static_assert(checksum::sha_compute<sha256>("abc"sv)[0] == 0xBA);

std::array<std::uint8_t, 32> digest = checksum::sha_compute<sha256>(image);
auto wide = checksum::sha_compute<sha512>(file_contents);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed. The digest is a `std::array<std::uint8_t, N>` in the byte order of
the standard, the same bytes `sha256sum` prints.

## Choosing a variant for your CPU

No preset has a kernel yet: every preset runs the same portable code. On 64-bit CPUs SHA-512 is 1.3-1.5× faster
than SHA-256, and SHA-512/256 gives a 32-byte digest at the speed of SHA-512. On 32-bit cores SHA-256 is faster.

```cpp
// Data in pieces, as a bootloader reads flash.
constexpr checksum::sha_engine<sha256> sha;
checksum::accumulator running{sha};
for(auto page : pages) {
  running.update(page);
}
bool valid = running.value() == expected;
```

## You don't know the CPU, or you need broad compatibility

You need nothing extra today: the dispatch engine always runs the portable loop. Use it if you dispatch every
algorithm, and your code will pick up CPU kernels when they arrive.

```cpp
static checksum::sha_dispatch_engine<sha256> const sha;
auto digest = sha.compute(data);
```

## Reference

### API

| Name | Description |
| --- | --- |
| `sha_variant` | `sha1`, `sha224`, `sha256`, `sha384`, `sha512`, `sha512_224`, `sha512_256`. |
| `sha_compute<Variant, Isa>(data)` | The digest of `data`. |
| `sha_engine<Variant, Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)` and the static `preset()`. |
| `sha_state<Variant>` | The hash value, the byte count and an unfinished block. A plain value; `==` compares what the digest depends on. |
| `sha_dispatch_engine<Variant>` | The same members, with the constructor `(isa_set cpu = detect_isa())`. |

`Isa` defaults to `isa::generic`. `accumulator` works with every engine.

- A message may be split anywhere and gives the same digest, up to 2^61 − 1 bytes.
- There is no `resume`, because a digest does not determine the state.
- The state is 96 bytes for SHA-1, 104 for SHA-224 and SHA-256 and 200 for the others, and it is passed by value.

### Performance

- At 4 KiB, SHA-256 runs about 475 MB/s on an i7-13700K and 1.3 MB/s on an 80 MHz Cortex-M4.
- On a Cortex-M4 the code of SHA-256 takes about 1.2 KB at `-O2` and 1 KB at `-Os`.

Full figures are in [performance.md](performance.md#sha-1-and-sha-2).

### Limitations

- SHA-1 is broken for collisions: use it only where a format requires it, such as Git object names.
- HMAC and other keyed constructions are not provided, and the code is not written to resist timing attacks on secret
  data. Hash public data, such as firmware images and downloads.
- SHA-3 and the bit-oriented messages of FIPS 180-4 (lengths that are not whole bytes) are not provided.
