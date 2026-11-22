# Fletcher-16, -32 and -64

Fletcher checksums of three widths, in `checksum/fletcher.hpp`. They need no tables, and every function is `constexpr`
and `noexcept`. Adler-32 and ZFS fletcher4 have their own guides. Despite its name, fletcher4 is a different algorithm:
four sums modulo 2^64, where Fletcher-16, -32 and -64 keep two sums modulo 2^(n/2)−1.

## The simplest way

```cpp
#include <checksum/fletcher.hpp>
using namespace std::literals;

static_assert(checksum::fletcher_compute<16>("abcde"sv) == 0xC8F0);
static_assert(checksum::fletcher_compute<32>("abcde"sv) == 0xF04FC729);
static_assert(checksum::fletcher_compute<64>("abcde"sv) == 0xC8C6C527646362C6);

std::uint32_t value = checksum::fletcher_compute<32>(data);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## You know your target platform

When you build for a known CPU, name its preset as a template argument after the width. The program then runs that
preset's kernel directly, with no detection and no compiler flags. On x86-64, `isa::x86_avx2` is 1.8-2.5× faster than the
SSE2 kernel of the default for Fletcher-16 and -32, and has the only Fletcher-64 kernel. On Cortex-M4/M7/M33,
`isa::thumb2_dsp` doubles the speed of Fletcher-16.

```cpp
std::uint32_t value = checksum::fletcher_compute<32, checksum::isa::x86_avx2>(data);

// Data in pieces.
constexpr checksum::fletcher_engine<32, checksum::isa::x86_avx2> fletcher32;
checksum::accumulator running{fletcher32};
std::uint32_t frame_checksum = running.update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build.
Make it once, as a `static` or global object.

```cpp
static checksum::fletcher_dispatch_engine<32> const fletcher32;
std::uint32_t value = fletcher32.compute(data);
checksum::isa chosen = fletcher32.preset();
```

## Reference

### API

`Width` is 16, 32 or 64; the result is a `std::uint16_t`, `std::uint32_t` or `std::uint64_t`.

| Name | Description |
| --- | --- |
| `fletcher_compute<Width, Isa>(data)` | The checksum of `data`. |
| `fletcher_engine<Width, Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)`, `resume(checksum)` and the static `preset()`. |
| `fletcher_state<Width>` | The two sums and the bytes of an unfinished block. A plain value. |
| `fletcher_dispatch_engine<Width>` | The same members, with the constructor `(isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- Fletcher-16 sums bytes, Fletcher-32 16-bit words and Fletcher-64 32-bit words, read little-endian on every target. A
  last partial word is padded with zero bytes.
- A message may be split anywhere, even inside a word, and gives the same checksum. The empty message gives 0.
- `fletcher.update(fletcher.resume(stored), data)` continues from a stored checksum. For Fletcher-32 and -64 that is
  correct only after whole words, so keep the state otherwise.

### Presets and kernels

| Preset | Fletcher-16 | Fletcher-32 | Fletcher-64 |
| --- | --- | --- | --- |
| `x86_avxvnni` | `vpdpbusd`, from 64 B | as AVX2 | as AVX2 |
| `x86_avx2`, `x86_vpclmul` | 32-byte vectors, from 64 B | from 128 B | from 384 B |
| `generic`, `x86_pclmul` on x86-64 | 16-byte SSE2 vectors, from 64 B | from 128 B | portable |
| every preset on little-endian AArch64, `generic` included | NEON, from 64 B | from 128 B | from 256 B |
| `thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON, from 64 B | from 128 B | from 256 B |
| `thumb2_dsp` on little-endian 32-bit Arm (Cortex-M4/M7/M33) | `usada8` + `smlad`, from 64 B | portable | portable |
| `riscv_v` on RV64, tested in QEMU only | Vectors of any length, from 64 B | from 128 B | from 256 B |
| all others, big-endian included | portable | portable | portable |

The byte kernels are shared with Adler-32.

### Performance

- Pass a few KiB per `update` call, and at least the kernel's minimum of 64-384 bytes.
- The default has no Fletcher-64 kernel, so the compiler matters: at 4 KiB Clang's portable loop runs 1.8× slower than
  GCC's.
- On Cortex-M, wider is faster: at 4 KiB on an 80 MHz Cortex-M4, Fletcher-64 runs 46 MB/s, Fletcher-32 40 MB/s and
  Fletcher-16 with its DSP kernel 30 MB/s. Fletcher-32 and -64 are 20-22 % slower from an odd address.

Full figures are in [performance.md](performance.md#fletcher-16--32-and--64).

### Limitations

- A block of all one bits counts the same as a block of zeros, so error detection is weaker than a CRC's.
- Words are read little-endian only. A protocol that sends big-endian words needs them swapped first.
