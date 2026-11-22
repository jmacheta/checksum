# fletcher4

fletcher4, the checksum of the ZFS file system. On a little-endian host the values equal OpenZFS `fletcher_4_native`,
and on a big-endian host `fletcher_4_byteswap`. It is in `checksum/fletcher4.hpp`, needs no tables, and every function
is `constexpr` and `noexcept`.

## The simplest way

```cpp
#include <checksum/fletcher4.hpp>
using namespace std::literals;

static_assert(checksum::fletcher4_compute("abcdefgh"sv) ==
              checksum::fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});

checksum::fletcher4_value value = checksum::fletcher4_compute(data);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.
The result is a `std::array` of the four 64-bit sums, in the word order of the ZFS `zio_cksum_t`.

## You know your target platform

When you build for a known CPU, name its preset as a template argument. The program then runs that preset's kernel
directly, with no detection and no compiler flags. On x86-64, `isa::x86_avx2` hashes a 4 KiB block at about
36 000 MB/s, 1.5× the default and on par with OpenZFS.

```cpp
checksum::fletcher4_value value = checksum::fletcher4_compute<checksum::isa::x86_avx2>(data);

// Data in pieces.
constexpr checksum::fletcher4_engine<checksum::isa::x86_avx2> fletcher4;
checksum::accumulator running{fletcher4};
checksum::fletcher4_value block_checksum = running.update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build.
Make it once, as a `static` or global object.

```cpp
static checksum::fletcher4_dispatch_engine const fletcher4;
checksum::fletcher4_value value = fletcher4.compute(data);
checksum::isa chosen = fletcher4.preset();
```

## Reference

### API

| Name | Description |
| --- | --- |
| `fletcher4_compute<Isa>(data)` | The checksum of `data`. |
| `fletcher4_value` | The checksum, `std::array<std::uint64_t, 4>`. |
| `fletcher4_engine<Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)`, `resume(checksum)` and the static `preset()`. |
| `fletcher4_state` | The four sums and the bytes of an unfinished word. A plain value. |
| `fletcher4_dispatch_engine` | The same members, with the constructor `(isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- Words are 32 bits, read little-endian on every target. ZFS checksums whole words only; here a last partial word is
  padded with zero bytes, so `"abc"` and `"abc\0"` give the same checksum.
- A message may be split anywhere, even inside a word, and gives the same checksum. The sums wrap modulo 2^64, as in
  ZFS.
- `fletcher4.update(fletcher4.resume(stored), data)` continues from a stored checksum after any whole number of words.

### Presets and kernels

Short inputs run a word loop. Longer ones run four interleaved lanes, which a kernel keeps in vector registers.

| Preset | Lanes |
| --- | --- |
| `x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | One 256-bit vector per sum, from 128 B |
| `generic`, `x86_pclmul` on x86-64 | Two 128-bit vectors per sum, from 192 B |
| every preset on little-endian AArch64, `generic` included | NEON, from 192 B |
| `thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON, from 192 B |
| `riscv_v` on RV64 | One register group per sum, from 192 B, tested in QEMU only |
| other 64-bit targets: RISC-V without `riscv_v`, big-endian AArch64 | Portable lanes, from 256 B |
| other 32-bit targets, Cortex-M included | Word loop only |

### Performance

- ZFS blocks are long enough to always run the lanes.
- On an 80 MHz Cortex-M4 the word loop runs about 25 MB/s.

Full figures are in [performance.md](performance.md#fletcher4).

### Limitations

- Leading zero words do not change the checksum, and the sums of a short message stay small. Use a CRC where errors must
  be detected reliably.
- Words are read little-endian only. To match `fletcher_4_native` on a big-endian host, swap the words first.
