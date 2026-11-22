# Internet checksum

The Internet checksum of RFC 1071, as used by IPv4, ICMP, UDP and TCP. It is in `checksum/internet.hpp`, needs no
tables, and every function is `constexpr` and `noexcept`.

## The simplest way

```cpp
#include <checksum/internet.hpp>
using namespace std::literals;

static_assert(checksum::internet_compute("123456789"sv) == 0xF62A);

std::uint16_t sum = checksum::internet_compute(data);
bool valid = checksum::internet_compute(received_header) == 0;
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.
A header that holds its correct checksum sums to 0, which is how a receiver verifies it.

## You know your target platform

When you build for a known CPU, such as servers with AVX2, name its preset as a template argument. The program then runs
that preset's kernel directly, with no detection and no compiler flags. On x86-64, AVX2 is about 1.8× faster than the
default from 1500 bytes. On AArch64 the default already has the NEON kernel, and Cortex-M3/M4/M7/M33 get theirs with
`isa::thumb2`.

```cpp
std::uint16_t sum = checksum::internet_compute<checksum::isa::x86_avx2>(data);

// A UDP checksum from its pieces, without copying them into one buffer.
constexpr checksum::internet_engine<checksum::isa::x86_avx2> internet;
checksum::accumulator running{internet};
std::uint16_t udp_checksum = running.update(pseudo_header).update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build.
Make it once, as a `static` or global object.

```cpp
static checksum::internet_dispatch_engine const internet;
std::uint16_t sum = internet.compute(data);
checksum::isa chosen = internet.preset();
```

## Reference

### API

| Name | Description |
| --- | --- |
| `internet_compute<Isa>(data)` | The checksum of `data`, a `std::uint16_t`. |
| `internet_engine<Isa>` | An empty engine with `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)`, `resume(checksum)` and the static `preset()`. |
| `internet_state` | The running sum and whether an odd number of bytes was folded. A plain value. |
| `internet_dispatch_engine` | The same members, with the constructor `(isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Isa` defaults to `isa::generic`, and no function has preconditions. `accumulator` works with every engine.

- A message may be split anywhere, even inside a 16-bit word, and gives the same checksum.
- The result is a number; write it most significant byte first.
- The empty message and an all-zero message give 0xFFFF.
- `internet.update(internet.resume(stored), data)` continues from a stored checksum, as in the RFC 1624 incremental
  update. It is correct only after an even number of bytes, so keep the state otherwise.

### Presets and kernels

| Preset | Kernel |
| --- | --- |
| `x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | 32-bit halves summed in 64-bit vector lanes, from 512 B |
| every preset on little-endian AArch64, `generic` included | NEON `uadalp`, from 512 B |
| `thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON, from 192 B |
| `thumb2`, `thumb2_dsp` (Cortex-M3/M4/M7/M33), and the NEON presets on big-endian | `ldm` + `adcs` assembly, from 192 B, at any alignment |
| `riscv_v` | Widening vector add, any vector length, from 64 B |
| all others | Portable loop of two add-with-carry chains |

### Performance

- The portable loop is already fast: it checksums 20-byte IPv4 headers at about 10 000 MB/s on x86-64.
- On Cortex-M, the `ldm` kernel runs at full speed from any address, while the portable loop is up to 2.5× slower when a
  header starts at an odd or 2-modulo-4 address.
- A kernel adds 590-750 bytes of code.

Full figures are in [performance.md](performance.md#internet-checksum).

### Limitations

- UDP sends a computed 0 as 0xFFFF. That is protocol logic and stays in the caller.
