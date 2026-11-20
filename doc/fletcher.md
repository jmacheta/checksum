# Fletcher checksum user guide

Fletcher-16, Fletcher-32 and Fletcher-64, at compile time or at run time. One header, `checksum/fletcher.hpp`; no
tables. Adler-32 and ZFS fletcher4 have their own guides: [adler32.md](adler32.md), [fletcher4.md](fletcher4.md).

## 1. Quick start

The three ways to compute, the same in every guide: in a constant expression, with an instruction-set preset named in
the program (section 5), and with the preset chosen for the running CPU (section 5.1). `data`, `header` and `payload`
are spans or other ranges of bytes.

```cpp
#include <checksum/fletcher.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::fletcher_compute<16>("abcde"sv) == 0xC8F0);
static_assert(checksum::fletcher_compute<32>("abcde"sv) == 0xF04FC729);
static_assert(checksum::fletcher_compute<64>("abcde"sv) == 0xC8C6C527646362C6);

// Explicit preset: AVX2 where this build has it (x86-64), isa::generic elsewhere.
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  std::uint32_t value = checksum::fletcher_compute<32, avx2>(data);
  constexpr checksum::fletcher_engine<32, avx2> fletcher32;
  std::uint32_t frame_checksum = fletcher32.finalize(fletcher32.update(fletcher32.update(fletcher32.initial(), header), payload));
}

// Run-time dispatch: the best preset of this build for the running CPU, chosen once.
static checksum::fletcher_dispatch_engine<32> const fletcher32;
checksum::isa chosen = fletcher32.preset();
checksum::accumulator running{fletcher32};
std::uint32_t frame_checksum = running.update(header).update(payload).value();
```

`examples/fletcher` has complete programs: checksums at compile time, a file checksum computed in chunks, and a frame
check.

## 2. Definition

Fletcher-N reads the message as blocks of N/2 bits and keeps two sums modulo M = 2^(N/2) − 1, both starting at 0:
`sum1` adds each block, `sum2` adds `sum1` after each block. The checksum is `(sum2 << N/2) | sum1`.

| Checksum | Block | M | Result |
| --- | --- | --- | --- |
| Fletcher-16 | 1 byte | 255 | `std::uint16_t` |
| Fletcher-32 | 16-bit little-endian word | 65535 | `std::uint32_t` |
| Fletcher-64 | 32-bit little-endian word | 2^32 − 1 | `std::uint64_t` |

A last incomplete block is padded with zero bytes. The values match the Wikipedia article "Fletcher's checksum",
which reads words little-endian.

## 3. API

`Width` is 16, 32 or 64.

| Name | What it does |
| --- | --- |
| `fletcher_engine<Width, Isa>` | The engine, an empty object. `initial()` is the empty message, `update(state, data)` folds `data` into `state` and returns the new state, `finalize(state)` is the checksum (an unfinished block padded with zero bytes), `resume(checksum)` the state of a message with that checksum (section 4), `compute(data)` is `finalize(update(initial(), data))`, and the static `preset()` is `Isa`. `value_type` is the checksum type. |
| `fletcher_state<Width>` | `sum1`, `sum2` and `block_offset` (bytes of an unfinished block already in `sum1`). A plain value; the default is the empty message. `sum_type` is the type of one sum. |
| `fletcher_compute<Width, Isa>(data)` | `fletcher_engine<Width, Isa>{}.compute(data)`. |
| `fletcher_dispatch_engine<Width>` | The same members as `fletcher_engine<Width>`, made with `(cpu = detect_isa())`; `preset()`, not static, names the preset chosen at run time (section 5.1). |
| `accumulator<Engine>` | `checksum::accumulator running{engine}` (`checksum/engine.hpp`, shared by every algorithm): a pointer to the engine and a state, with `update(data)` (chainable), `value()`, `state()` and `reset()`. The engine must outlive it. |

`Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and
`noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`,
`signed char` or `char8_t`; arrays of `char` are rejected so a literal's `'\0'` is never summed (pass
`std::string_view`). Every state value is valid, so nothing has preconditions: a sum equal to M counts as 0, and
`block_offset` is taken modulo the block size.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a block; the result is the same as for one call.
- **Zero:** the empty message and an all-zero message give 0.
- **Ones and zeros:** a sum never equals M, so a block of all one bits counts as a block of zeros. For Fletcher-16,
  bytes 0x00 and 0xFF give the same checksum, and neither byte of the result is ever 0xFF.
- **Byte order:** words are read little-endian on every target; the result is a number.
- **Continuing from a stored checksum:** `fletcher.update(fletcher.resume(value), data)` continues a message whose
  checksum is `value`. Fletcher-32 and -64 continue correctly only after whole blocks (a multiple of 2 or 4 bytes),
  since the checksum counts an unfinished block as padded; keep the state otherwise. Fletcher-16 continues after any
  length.

## 5. CPU acceleration

The portable loop adds four blocks per step into the widest native integer and reduces only when a sum could
overflow. From a minimum size per call, a kernel sums whole vectors of blocks. Presets are explained in
[Instruction-set presets](../README.md#instruction-set-presets).

| Preset | Fletcher-16 | Fletcher-32 | Fletcher-64 |
| --- | --- | --- | --- |
| `isa::x86_avxvnni` | `vpdpbusd`, from 64 B | as AVX2 | as AVX2 |
| `isa::x86_avx2`, `x86_vpclmul` | 32-byte vectors, from 64 B | from 128 B | from 384 B |
| `isa::generic`, `x86_pclmul` on x86-64 | 16-byte vectors, from 64 B | from 128 B | portable loop |
| every preset of little-endian AArch64 (GCC, Clang, MSVC, clang-cl), `isa::generic` included | NEON from 64 B; 64 bytes per iteration from 320 B | from 128 B | from 256 B |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm (GCC, Clang) | NEON, from 64 B | from 128 B | from 256 B |
| `isa::thumb2_dsp` on little-endian 32-bit Arm (GCC, Clang; for Cortex-M4/M7/M33) | `usada8` + `smlad`, from 64 B | portable loop | portable loop |
| everything else: `isa::thumb2`, `riscv_zbc`, `riscv_v`, big-endian targets | portable loop | portable loop | portable loop |

Adler-32 uses the same byte kernels. Decisions per preset:
[design/acceleration.md](design/acceleration.md#fletcher-and-adler-32).

### 5.1 Choosing the preset at run time

The dispatch engine of section 1 asks the CPU once, when it is made, and from then on runs the best variant this build
has for it. `preset()` names the chosen preset; on RISC-V, where no preset has a kernel, it is `generic`. An explicit preset chooses instead, e.g. from a configuration:
`checksum::fletcher_dispatch_engine<32>(checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time
where this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on
top of the variant. Detection does not run in constant expressions: there, call `fletcher_compute<32>` or give the
engine an explicit preset. Details and the list of engines: [Instruction-set
presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#fletcher-16--32-and--64).

- **Pass data in chunks of a few KiB**, at least the kernel's minimum size (64-384 B) per `update` call.
- **On x86-64 use `isa::x86_avx2`** where the CPU has it: 2.4× faster than the SSE2 kernel of `isa::generic` for
  Fletcher-16 and -32 at 4 KiB, and the only Fletcher-64 kernel. With Clang a kernel matters more: its portable loop is
  2.1-2.3× slower than GCC's.
- **On Cortex-M wider is faster:** at 4 KiB on an 80 MHz Cortex-M4 Fletcher-64 runs 44 MB/s and Fletcher-32 38 MB/s,
  against 30 MB/s for Fletcher-16 with its DSP kernel. Align buffers for Fletcher-32 and -64: from an odd address they
  are 11-22 % slower.

## 7. Limitations

- **Weaker error detection than a CRC:** a block of zeros and a block of ones look the same. Use a CRC where errors
  must be detected reliably.
- **Little-endian words only:** a protocol that reads Fletcher-32 or -64 blocks big-endian needs its bytes swapped
  first.
