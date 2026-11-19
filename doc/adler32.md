# Adler-32 user guide

Adler-32, the checksum of the zlib format (RFC 1950), at compile time or at run time; values equal zlib's
`adler32()`. One header, `checksum/adler32.hpp`; no tables.

## 1. Quick start

```cpp
#include <checksum/adler32.hpp>
using namespace std::literals;

// One call.
std::uint32_t value = checksum::adler32_compute(std::span(buffer));

// Incremental: a message may be split anywhere.
checksum::adler32_state state;
state = checksum::adler32_update(state, first_part);
state = checksum::adler32_update(state, second_part);
std::uint32_t stream_checksum = checksum::adler32_finalize(state);

// Continue from a stored checksum.
std::uint32_t extended = checksum::adler32_update(stream_checksum, third_part);

// Compile time.
static_assert(checksum::adler32_compute("123456789"sv) == 0x091E01DE);
static_assert(checksum::adler32_compute("Wikipedia"sv) == 0x11E60398);
```

`examples/adler32` has complete programs: a checksum computed at compile time, checking the Adler-32 trailer of a
zlib stream, and extending a stored checksum when data is appended.

## 2. Definition

Two sums modulo 65521, the largest prime below 2^16: `sum1` starts at 1 and adds each byte, `sum2` starts at 0 and
adds `sum1` after each byte. The checksum is `(sum2 << 16) | sum1`. It differs from Fletcher-32
([fletcher.md](fletcher.md)) in the prime modulus, the start value 1 and the 8-bit blocks.

## 3. API

| Name | What it does |
| --- | --- |
| `adler32_state` | `sum1` and `sum2`. A plain value; the default (`sum1 = 1`, `sum2 = 0`) is the empty message. |
| `adler32_update<Isa>(state, data)` | Folds `data` into `state` and returns the new state. |
| `adler32_update<Isa>(checksum, data)` | Folds `data` into the message whose checksum is `checksum` (a `std::uint32_t`) and returns the new checksum. |
| `adler32_finalize(state)` | The checksum. |
| `adler32_compute<Isa>(data)` | `adler32_finalize(adler32_update<Isa>({}, data))`. |
| `adler32_dispatch_engine` | `compute` and `update` with the preset chosen at run time. |

`Isa` is an instruction-set preset, `isa::generic` by default (section 5). All functions are `constexpr` and
`noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`, `char`, `unsigned char`,
`signed char` or `char8_t`; arrays of `char` are rejected so a literal's `'\0'` is never summed (pass
`std::string_view`). Every state value is valid, so nothing has preconditions: sums from 65521 up count modulo 65521.

## 4. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Empty message:** gives 1.
- **Byte order:** the result is a number; zlib stores it most significant byte first.
- **Continuing from a stored checksum:** the checksum holds both sums, so `adler32_update(value, data)` continues any
  message after any length, a checksum from zlib included, as zlib's `adler32(adler, buf, len)` does. `value` must be a
  `std::uint32_t`, so that `adler32_update({}, data)` still starts from the empty state.

## 5. CPU acceleration

The portable loop adds four bytes per step into native-width sums and reduces only when a sum could overflow, without
a division. From 64 bytes per call, Adler-32 runs the byte kernel of Fletcher-16 with modulus 65521. Presets are
explained in [Instruction-set presets](../README.md#instruction-set-presets).

| Preset | Kernel |
| --- | --- |
| `isa::x86_avxvnni` | `vpdpbusd`, 128 bytes per iteration |
| `isa::x86_avx2`, `x86_vpclmul` | 32-byte vectors |
| `isa::generic`, `x86_pclmul` on x86-64 | 16-byte vectors |
| every preset of little-endian AArch64 with GCC or Clang, `isa::generic` included | NEON; 64 bytes per iteration from 320 B |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON |
| `isa::thumb2_dsp` on little-endian 32-bit Arm (for Cortex-M4/M7/M33) | `usada8` + `smlad`, 8 bytes per iteration |
| everything else: `isa::thumb2`, `riscv_zbc`, `riscv_v`, big-endian targets, MSVC ARM64 | portable loop |

Decisions per preset: [design/acceleration.md](design/acceleration.md#fletcher-and-adler-32).

### 5.1 Choosing the preset at run time

A dispatch engine asks the CPU once, when it is made, and from then on runs the best variant this build has for it:

```cpp
#include <checksum/adler32.hpp>

// Made once, during program start-up: detect_isa() queries the CPU or the operating system.
static checksum::adler32_dispatch_engine const adler32;

std::uint32_t stream_checksum(std::span<std::byte const> data) { return adler32.compute(data); }
```

`adler32.preset()` names the chosen preset. An explicit preset chooses instead, e.g. from a configuration:
`checksum::adler32_dispatch_engine(checksum::isa::x86_avx2)` runs that variant, falling back one rung at a time where
this build lacks it; it does not check the CPU. A call costs one switch over the presets (one or two cycles) on top of
the variant. Detection does not run in constant expressions: there, call `adler32_compute` or give the engine an
explicit preset. Details and the list of engines: [Instruction-set presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#adler-32).

- **Pass data in chunks of a few KiB**, at least 64 bytes per `adler32_update` call.
- **On x86-64 choose the preset for the CPU:** at 4 KiB `isa::generic` runs about 23 000 MB/s, `isa::x86_avx2` about
  58 000 and `isa::x86_avxvnni` about 85 000, 1.13-1.17× zlib-ng.
- **On Cortex-M4/M7/M33 use `isa::thumb2_dsp`:** 2× the portable loop at 4 KiB, from any start address.

## 7. Limitations

- **Weak on short messages:** with few bytes `sum1` stays far below 65521, so most of the 32 bits are unused. Use a CRC
  for short messages where errors must be detected reliably.
