# Adler-32

Adler-32 as defined by zlib (RFC 1950), with values equal to zlib's `adler32()`. It is in `checksum/adler32.hpp`, needs
no tables, and every function is `constexpr` and `noexcept`.

## The simplest way

```cpp
#include <checksum/adler32.hpp>
using namespace std::literals;

static_assert(checksum::adler32_compute("Wikipedia"sv) == 0x11E60398);

std::uint32_t value = checksum::adler32_compute(data);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

## You know your target platform

When you build for a known CPU, name its preset as a template argument. The program then runs that preset's kernel
directly, with no detection and no compiler flags. At 4 KiB on x86-64, `isa::x86_avx2` is 2.5× faster than the SSE2
kernel of the default, and `isa::x86_avxvnni` 4×. On Cortex-M4/M7/M33, `isa::thumb2_dsp` nearly doubles the speed.

```cpp
std::uint32_t value = checksum::adler32_compute<checksum::isa::x86_avx2>(data);

// Data in pieces.
constexpr checksum::adler32_engine<checksum::isa::x86_avx2> adler32;
checksum::accumulator running{adler32};
std::uint32_t stream_checksum = running.update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build.
Make it once, as a `static` or global object.

```cpp
static checksum::adler32_dispatch_engine const adler32;
std::uint32_t value = adler32.compute(data);
checksum::isa chosen = adler32.preset();
```

## Reference

### API

| Name | Description |
| --- | --- |
| `adler32_compute<Isa>(data)` | The checksum of `data`, a `std::uint32_t`. |
| `adler32_engine<Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)`, `resume(checksum)` and the static `preset()`. |
| `adler32_state` | The two sums, `sum1` and `sum2`. A plain value. |
| `adler32_dispatch_engine` | The same members, with the constructor `(isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere and gives the same checksum. The empty message gives 1.
- The result is a number; zlib stores it most significant byte first.
- `adler32.update(adler32.resume(stored), data)` continues from any stored checksum, zlib's included, after any length.

### Presets and kernels

| Preset | Kernel |
| --- | --- |
| `x86_avxvnni` | `vpdpbusd`, 128 bytes per iteration, from 64 B |
| `x86_avx2`, `x86_vpclmul` | 32-byte vectors, from 64 B |
| `generic`, `x86_pclmul` on x86-64 | 16-byte SSE2 vectors, from 64 B |
| every preset on little-endian AArch64, `generic` included | NEON from 64 B, 64 bytes per iteration from 320 B |
| `thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON, from 64 B |
| `thumb2_dsp` on little-endian 32-bit Arm (Cortex-M4/M7/M33) | `usada8` + `smlad`, from 64 B, at any alignment |
| `riscv_v` on RV64 | Vectors of any length, from 64 B, tested in QEMU only |
| all others, big-endian included | Portable loop |

The kernels are shared with Fletcher-16, with the modulus 65521.

### Performance

- At 4 KiB on x86-64, the default runs about 13 000 MB/s, `x86_avx2` 68 000 and `x86_avxvnni` 110 000.
- Pass at least 64 bytes per `update` call, and preferably a few KiB, so that the kernels run.

Full figures are in [performance.md](performance.md#adler-32).

### Limitations

- Adler-32 is weak on short messages, where `sum1` stays far below the modulus. Use a CRC where errors must be detected
  reliably.
