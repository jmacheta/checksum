# fletcher4 user guide

fletcher4, the checksum of the ZFS file system, at compile time or at run time. One header,
`checksum/fletcher4.hpp`; no tables. Fletcher-16, -32 and -64 have their own guide: [fletcher.md](fletcher.md).

## 1. Quick start

The three ways to compute, the same in every guide: in a constant expression, with an instruction-set preset named in
the program (section 5), and with the preset chosen for the running CPU (section 5.1). `data`, `header` and `payload`
are spans or other ranges of bytes.

```cpp
#include <checksum/fletcher4.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::fletcher4_compute("abcdefgh"sv) ==
              checksum::fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});

// Explicit preset: AVX2 where this build has it (x86-64), isa::generic elsewhere.
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  checksum::fletcher4_value value = checksum::fletcher4_compute<avx2>(data);
  constexpr checksum::fletcher4_engine<avx2> fletcher4;
  checksum::fletcher4_value block_checksum = fletcher4.finalize(fletcher4.update(fletcher4.update(fletcher4.initial(), header), payload));
}

// Run-time dispatch: the best preset of this build for the running CPU, chosen once.
static checksum::fletcher4_dispatch_engine const fletcher4;
checksum::isa chosen = fletcher4.preset();
checksum::accumulator running{fletcher4};
checksum::fletcher4_value block_checksum = running.update(header).update(payload).value();
```

`examples/fletcher4` has complete programs: a checksum computed at compile time, a file read in chunks, and verifying
a 4 KiB block against its stored checksum as ZFS does.

## 2. Definition

The message is read as 32-bit little-endian words; four 64-bit sums start at 0. For each word `sum1` adds the word,
`sum2` adds `sum1`, `sum3` adds `sum2` and `sum4` adds `sum3`, all modulo 2^64. The checksum is
`{sum1, sum2, sum3, sum4}`, the word order of the ZFS `zio_cksum_t`.

On a little-endian host the result equals OpenZFS `fletcher_4_native`, on a big-endian host `fletcher_4_byteswap`.
ZFS checksums whole words only; here a last partial word is padded with zero bytes, so `"abc"` gives the same checksum
as `"abc\0"`.

## 3. API

| Name | What it does |
| --- | --- |
| `fletcher4_engine<Isa>` | The engine, an empty object. `initial()` is the empty message, `update(state, data)` folds `data` into `state` and returns the new state, `finalize(state)` is the checksum (an unfinished word padded with zero bytes), `resume(checksum)` the state of a message with that checksum (section 4), `compute(data)` is `finalize(update(initial(), data))`, and the static `preset()` is `Isa`. |
| `fletcher4_state` | `sum1` to `sum4` and `word_offset` (bytes of an unfinished word already in `sum1`). A plain value; the default is the empty message. |
| `fletcher4_value` | The checksum: `std::array<std::uint64_t, 4>` holding `sum1` to `sum4`. |
| `fletcher4_compute<Isa>(data)` | `fletcher4_engine<Isa>{}.compute(data)`. |
| `fletcher4_dispatch_engine` | The same members as `fletcher4_engine`, made with `(cpu = detect_isa())`; `preset()`, not static, names the preset chosen at run time (section 5.1). |
| `accumulator<Engine>` | `checksum::accumulator running{engine}` (`checksum/engine.hpp`, shared by every algorithm): a pointer to the engine and a state, with `update(data)` (chainable), `value()`, `state()` and `reset()`. The engine must outlive it. |

`Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and
`noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`,
`signed char` or `char8_t`; arrays of `char` are rejected so a literal's `'\0'` is never summed (pass
`std::string_view`). Every state value is valid, so nothing has preconditions: `word_offset` is taken modulo 4.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a word; the result is the same as for one call.
- **Zero:** the empty message and an all-zero message give `{0, 0, 0, 0}`, so leading zero words do not change the
  checksum.
- **Wraparound:** the sums wrap modulo 2^64, as in ZFS; there is no length limit.
- **Byte order:** words are read little-endian on every target; the result is four numbers.
- **Continuing from a stored checksum:** the checksum holds all four sums, so `fletcher4.update(fletcher4.resume(value),
  data)` continues a message after any whole number of words; keep the state after a partial word.

## 5. CPU acceleration

The word loop is one chain of four dependent additions per word. Longer inputs run four interleaved lanes, combined
exactly with fixed weights, in vector registers where a kernel exists. Presets are explained in
[Instruction-set presets](../README.md#instruction-set-presets).

| Preset | Lanes |
| --- | --- |
| `isa::x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | One 256-bit vector per sum, from 128 B |
| `isa::generic`, `x86_pclmul` on x86-64 | Two 128-bit vectors per sum, from 192 B |
| every preset of little-endian AArch64, `isa::generic` included (MSVC too) | NEON, from 192 B |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm (GCC, Clang) | NEON, from 192 B |
| other 64-bit targets: `riscv_zbc`, `riscv_v`, big-endian AArch64 | Portable lanes, from 256 B |
| other 32-bit targets: `isa::generic`, `thumb2`, `thumb2_dsp`, big-endian 32-bit Arm | Word loop only, two words per iteration on 32-bit Arm without `thumb2_neon`/`thumb2_crc`, one word per iteration with `thumb2_neon`/`thumb2_crc` |

Decisions per preset: [design/acceleration.md](design/acceleration.md#fletcher4).

### 5.1 Choosing the preset at run time

The dispatch engine of section 1 asks the CPU once, when it is made, and from then on runs the best variant this build
has for it. `preset()` names the chosen preset; on RISC-V, where no preset has a kernel, it is `generic`. An explicit preset chooses instead, e.g. from a configuration:
`checksum::fletcher4_dispatch_engine(checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time where
this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on top of
the variant. Detection does not run in constant expressions: there, call `fletcher4_compute` or give the engine an
explicit preset. Details and the list of engines: [Instruction-set presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#fletcher4).

- **ZFS blocks always run the lanes** (from 128-256 B per call); with AVX2 a 4 KiB block hashes at about 29 000 MB/s,
  on par with OpenZFS.
- **On 32-bit targets without NEON**, Cortex-M included, the word loop runs about 20 MB/s on a 64 MHz Cortex-M4.
  Aligned buffers are 16 % faster than odd start addresses.

## 7. Limitations

- **Weak on short messages and zero words:** leading zero words do not change the checksum, and the sums of a short
  message stay small. Use a CRC where errors must be detected reliably.
- **Little-endian words only:** the equivalent of `fletcher_4_native` on a big-endian host needs its words swapped
  first.
- **Continuing from a stored checksum after whole words only:** a checksum does not record an unfinished word; keep the
  state after a partial word.
