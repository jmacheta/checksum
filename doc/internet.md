# Internet checksum user guide

The Internet checksum of RFC 1071, used by IPv4, ICMP, IGMP, UDP and TCP, at compile time or at run time. One header,
`checksum/internet.hpp`; no tables.

## 1. Quick start

The three ways to compute, the same in every guide: in a constant expression, with an instruction-set preset named in
the program (section 5), and with the preset chosen for the running CPU (section 5.1). `data`, `header` and `payload`
are spans or other ranges of bytes.

```cpp
#include <checksum/internet.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::internet_compute("123456789"sv) == 0xF62A);

// Explicit preset: AVX2 where this build has it (x86-64), isa::generic elsewhere.
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  std::uint16_t sum = checksum::internet_compute<avx2>(data);
  constexpr checksum::internet_engine<avx2> internet;
  std::uint16_t udp_checksum = internet.finalize(internet.update(internet.update(internet.initial(), header), payload));
}

// Run-time dispatch: the best preset of this build for the running CPU, chosen once.
static checksum::internet_dispatch_engine const internet;
checksum::isa chosen = internet.preset();
checksum::accumulator running{internet};
std::uint16_t udp_checksum = running.update(header).update(payload).value();
```

`examples/internet` has complete programs: an IPv4 header template filled in at compile time, an IPv4 header checksum
filled in and verified, a UDP checksum over pseudo-header, header and payload, and the RFC 1624 update after a TTL
decrement.

## 2. Definition

The message is read as 16-bit big-endian words, a last odd byte padded with a zero byte. The words are added in one's
complement arithmetic (each carry out of bit 15 added back into bit 0), and the checksum is the bitwise NOT of the sum.

## 3. API

| Name | What it does |
| --- | --- |
| `internet_engine<Isa>` | The engine, an empty object. `initial()` is the empty message, `update(state, data)` folds `data` into `state` and returns the new state, `finalize(state)` is the checksum `~state.sum`, `resume(checksum)` the state of a message with that checksum (section 4), `compute(data)` is `finalize(update(initial(), data))`, and the static `preset()` is `Isa`. |
| `internet_state` | The running sum (`sum`) and whether an odd number of bytes was folded (`odd`). A plain value; the default is the empty message. |
| `internet_compute<Isa>(data)` | `internet_engine<Isa>{}.compute(data)`. |
| `internet_dispatch_engine` | The same members as `internet_engine`, made with `(cpu = detect_isa())`; `preset()`, not static, names the preset chosen at run time (section 5.1). |
| `accumulator<Engine>` | `checksum::accumulator running{engine}` (`checksum/engine.hpp`, shared by every algorithm): a pointer to the engine and a state, with `update(data)` (chainable), `value()`, `state()` and `reset()`. The engine must outlive it. |

`Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and `noexcept`.
`data` is a `std::span<std::byte const>` or any `byte_range`: an input range of non-`volatile` `std::byte`, `char`,
`unsigned char`, `signed char` or `char8_t`; non-contiguous ranges are copied through a 64-byte stack buffer. Arrays of
`char` and `char8_t` are rejected so a string literal's `'\0'` is never included: pass `std::string_view`. Every
`internet_state` value is valid, so nothing has preconditions.

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a 16-bit word; the result is the same as for one call.
- **Zero:** the sum is 0 only if every byte is 0, otherwise a multiple of 0xFFFF becomes 0xFFFF. So the empty message
  and an all-zero message give 0xFFFF, and a message that contains its correct checksum gives 0: that is how a receiver
  verifies a packet.
- **Byte order:** the result is a number; write it most significant byte first. Every target reads the message the
  same way.
- **Continuing from a stored checksum:** `internet.update(internet.resume(value), data)` continues a message whose
  checksum is `value` (the running sum is `~value`). It is correct only after an even number of bytes, since the
  checksum does not record an odd last byte; keep the state otherwise.

## 5. CPU acceleration

The portable loop adds native words in two add-with-carry chains and needs no tables; it is fast on its own. Kernels
take over from a minimum size; presets are explained in [Instruction-set presets](../README.md#instruction-set-presets).

| Preset | Kernel |
| --- | --- |
| `isa::x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | 32-bit halves summed into 64-bit vector lanes, from 512 B |
| every preset of little-endian AArch64 (GCC, Clang, MSVC, clang-cl), `isa::generic` included | NEON `uadalp` into 64-bit lanes, from 512 B |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm (GCC, Clang) | The same NEON kernel, from 192 B |
| `isa::thumb2`, `thumb2_dsp` (and the NEON presets on big-endian), e.g. Cortex-M3/M4/M7/M33 | `ldm` + `adcs` inline assembly, any start address, from 192 B |
| `isa::riscv_v` | Widening vector add into 64-bit lanes, any vector length, from 64 B |
| everything else: `isa::generic` elsewhere, `x86_pclmul`, `riscv_zbc`, big-endian AArch64 | Portable loop |

Decisions per preset: [design/acceleration.md](design/acceleration.md#internet-checksum).

### 5.1 Choosing the preset at run time

The dispatch engine of section 1 asks the CPU once, when it is made, and from then on runs the best variant this build
has for it. `preset()` names the chosen preset; on RISC-V only `riscv_v`, the preset with a kernel, counts, so a CPU with Zbc alone runs `generic`. An explicit preset chooses instead, e.g. from a configuration:
`checksum::internet_dispatch_engine(checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time where
this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on top of
the variant. Detection does not run in constant expressions: there, call `internet_compute` or give the engine an
explicit preset. Details and the list of engines: [Instruction-set presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#internet-checksum).

- **Packet headers run the portable loop**, which is already fast: about 8 000 MB/s for a 20-byte IPv4 header on
  x86-64.
- **On x86-64 use `isa::x86_avx2`** where the CPU has it: 1.2-1.7× faster from 1500 B. AArch64 gets its kernel with
  `isa::generic`, Cortex-M3/M4/M7/M33 with `isa::thumb2`.
- **On Cortex-M the start address matters for the portable loop only:** from an odd or 2-modulo-4 address (an IP header
  behind a 14-byte Ethernet header) it is up to 2.5× slower than the kernel, which runs at the same speed from any
  address.
- **Flash budget:** a kernel adds 590-750 bytes of code on Arm and x86-64.

## 7. Limitations

- **UDP zero:** sending 0xFFFF instead of a computed 0 is protocol logic and stays in the caller.
