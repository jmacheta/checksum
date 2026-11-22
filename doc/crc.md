# CRC

Cyclic redundancy checks of any width from 1 to 64 bits, over byte- or bit-granular messages. Everything is in
`checksum/crc.hpp`, including a catalog of 112 named CRCs, and every function is `constexpr` and `noexcept`.

## The simplest way

```cpp
#include <checksum/crc.hpp>
using namespace std::literals;

static_assert(checksum::crc_compute<checksum::crc32>("123456789"sv) == 0xCBF43926);

std::uint32_t crc = checksum::crc_compute<checksum::crc32>(data);
std::uint16_t modbus = checksum::crc_compute<checksum::crc16_modbus>(data);
```

`data` is a `std::span<std::byte const>` or any range of bytes. Pass text as `std::string_view`: character arrays are
rejected so that a literal's `'\0'` is never hashed.

Every CRC of the [catalog](#catalog) is a named constant, and a CRC that is not there takes four lines to
[define](#custom-parameters). The engine and its table are built at compile time and placed in read-only data, so the
program contains no table-building code.

## You know your target platform

The default engine computes bit by bit and has no table, which suits a small microcontroller. Faster engines trade
memory for speed through a [strategy](#strategies): `crc_lut_byte` needs 1 KiB for CRC-32 and is about 5× faster, and
`crc_lut_sliced` needs 8 KiB and is faster again.

On top of that, `crc_lut_sliced` uses CPU instructions when you name a preset after the strategy. On x86-64,
`isa::x86_vpclmul` computes CRC-32 at about 86 000 MB/s from 4 KiB, against about 3 400 MB/s for the portable loop. The
program runs that kernel directly, with no detection and no compiler flags.

```cpp
using checksum::crc_lut_sliced, checksum::isa;
std::uint32_t crc = checksum::crc_compute<checksum::crc32, crc_lut_sliced, isa::x86_vpclmul>(data);

// Data in pieces.
constexpr auto const &crc32 =
    checksum::crc_engine_for<checksum::crc32, crc_lut_sliced, isa::x86_vpclmul>;
checksum::accumulator running{crc32};
std::uint32_t frame_crc = running.update(header).update(payload).value();
```

The CPU must support a named preset: running it on one that does not is undefined behavior. See
[Hardware acceleration](../README.md#hardware-acceleration).

## You don't know the CPU, or you need broad compatibility

A dispatch engine detects the CPU once and then runs the best preset of the build. Make it
once, as a `static` or global object.

```cpp
using crc32_dispatch = checksum::crc_dispatch_engine<std::uint32_t, checksum::crc_lut_sliced>;
static auto const crc32 = crc32_dispatch::create(checksum::crc32).value();
std::uint32_t crc = crc32.compute(data);
checksum::isa chosen = crc32.preset();
```

Unlike the other dispatch engines, this one is made by `create`, because its parameters may come from run time and be
invalid: `create` returns an empty `std::optional` then. You also name the register type and the strategy, as for
`crc_engine`. The engine holds a single table, which the loops of all presets share. Only `crc_lut_sliced` and
`crc_lut_braided` have kernels, so with any other strategy `preset()` is always `generic`.

## Reference

### Catalog

The catalog defines every CRC of width up to 64 from the "Catalogue of parametrised CRC algorithms". A name is the
catalog name in lower case, with `CRC-` written `crc` and other `/` and `-` written `_`: `crc32_iso_hdlc`,
`crc16_ibm_3740`, `crc15_can`. Each entry's comment lists its polynomial and other names.

Four common aliases are provided: `crc32` (CRC-32/ISO-HDLC), `crc32c` (CRC-32/ISCSI), `crc16_ccitt_false`
(CRC-16/IBM-3740) and `crc16_x25` (CRC-16/IBM-SDLC). There is no bare `crc64`, because the catalog's CRC-64 is ECMA-182
while common usage often means `crc64_xz`.

### Custom parameters

A CRC is a `crc_parameters` aggregate, which can be a template argument.

**`.polynomial` is in Koopman notation, not the normal form of data sheets:** CRC-16/MODBUS's 0x8005 is 0xC002 here.
Convert the normal form with `crc_polynomial::create(0x8005, 16)`.

```cpp
inline constexpr checksum::crc_parameters sensor_crc{
    .polynomial = 0xC002,  // x^16 + x^15 + x^2 + 1 in Koopman notation
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
};
std::uint16_t crc = checksum::crc_compute<sensor_crc>(data);
```

| Field | Meaning |
| --- | --- |
| `polynomial` | The generator polynomial, whose degree is the width. |
| `initial_value` | The register before the first bit, unreflected and in the direct form, as catalogs publish it. Default 0. |
| `reflect_input` | Whether each byte is fed least significant bit first. Default `false`. |
| `reflect_output` | Whether the result is reflected before the final XOR. Default `false`. |
| `final_xor` | The value XORed into the result, unreflected. Default 0. |

In Koopman notation bit i is the coefficient of x^(i + 1), and the x^0 term is implicit. `crc_polynomial::create`
returns an empty `std::optional` for an invalid polynomial.

For parameters known only at run time, `crc_engine<Register, Strategy>::create(parameters)` returns an engine in a
`std::optional`, which is empty when the width does not fit the register or a value does not fit the width.
`crc_engine_for` makes the same check at compile time.

```cpp
using byte_engine = checksum::crc_engine<std::uint64_t, checksum::crc_lut_byte>;
auto make_engine = [](checksum::crc_polynomial polynomial) {
  return byte_engine::create({.polynomial = polynomial});
};
auto engine = checksum::crc_polynomial::create(normal_form, width).and_then(make_engine);
```

### API

| Name | Description |
| --- | --- |
| `crc_compute<Parameters, Strategy, Isa>(data)` | The CRC of `data`, through `crc_engine_for`. |
| `crc_engine_for<Parameters, Strategy, Isa>` | An engine built at compile time with the smallest register that fits, as a constant in read-only data. |
| `crc_engine<Register, Strategy, Isa>` | An engine with `create(parameters)`, `initial()`, `update(state, data)`, `finalize(state)`, `compute(data)`, `resume(crc)` and the static `preset()`. `Register` is `std::uint8_t` to `std::uint64_t`. |
| `crc_dispatch_engine<Register, Strategy>` | The same members, with `create(parameters, isa_set cpu = detect_isa())`. Passing a preset as `cpu` runs it, or the next lower one the build has, without checking the CPU. |

`Strategy` defaults to `crc_lut_none` and `Isa` to `isa::generic`. All strategies give identical results.

- A message may be split anywhere and gives the same CRC.
- `update(state, data, bit_length)` and `compute(data, bit_length)` take a message whose length is not a whole number
  of bytes, such as the 83 bits of a CAN frame: `crc_engine_for<checksum::crc15_can>.compute(bits, 83)`. The last bits
  are taken in the CRC's bit order.
- `crc32.update(crc32.resume(stored), data)` continues from a stored CRC after any length.
- A state passed in must be below 2^width, and `bit_length` at most `8 * data.size()`. Both are checked by `assert`.
  With `NDEBUG`, a violation gives a wrong CRC but never undefined behavior.

### Strategies

| Strategy | Engine size, 8 / 16 / 32 / 64-bit register | Method |
| --- | --- | --- |
| `crc_lut_none` | 11 / 16 / 32 / 64 B | Bitwise, no table |
| `crc_lut_nibble` | 27 / 48 / 96 / 192 B | 16-entry table, two lookups per byte |
| `crc_lut_byte` | 267 / 528 / 1056 / 2112 B | 256-entry table, one lookup per byte |
| `crc_lut_sliced` | 2.2 / 4.2 / 8.2 / 16.2 KiB | Slicing-by-8, with CPU kernels |
| `crc_lut_braided` | 4.2 / 8.2 / 16.2 / 32.2 KiB | Slicing-by-8 over five interleaved streams from 128 B, with CPU kernels |

- **Microcontrollers short of memory:** `crc_lut_none` or `crc_lut_nibble`.
- **Microcontrollers with spare memory:** `crc_lut_byte`, or `crc_lut_sliced` for the best speed. Keep the engine in
  RAM if it fits, because table lookups from flash are 1.2-1.5× slower.
- **Desktop and server CPUs:** `crc_lut_sliced` with a preset. Without a kernel, `crc_lut_braided` is about twice as
  fast from 128 B on out-of-order cores, but slower on in-order ones.

Keep large engines in static storage, through `crc_engine_for` or a `static` object, and not on a small stack.

### Custom strategies

A strategy can drive a CRC peripheral or any other hardware. It is a type with three members, and the engine still
handles reflection, the initial value, the final XOR and bit tails:

- `template <class Register> using table_type`, a semiregular type holding whatever `update` needs;
- `template <class Register> static std::optional<table_type<Register>> make_table(crc_parameters const &)`, which runs
  once per engine and returns `std::nullopt` for parameters the hardware cannot handle;
- `template <class Register> static Register update(table_type<Register> const &, Register state, std::span<std::byte const>)`,
  which folds whole bytes into the state, reflected when the input is reflected.

[examples/crc/hardware_strategy.cpp](../examples/crc/hardware_strategy.cpp) drives the CRC unit of STM32 parts this way.
On an STM32L4A6 it matches `crc_lut_sliced` in RAM on long messages, with no table, but its per-call setup makes it
slower on short ones. A strategy without `constexpr` members works with `crc_engine::create` at run time only.

A strategy can also take the engine's preset. If its `update` has a second template parameter, `isa Isa`, the engine
calls `update<Register, Isa>`, and a dispatch engine chooses among all presets of the build; otherwise the strategy
always gets `generic`. A `static constexpr bool takes_preset` member overrides that choice.

### Presets and kernels

Only `crc_lut_sliced` and `crc_lut_braided` have kernels. With a kernel both run the same code, so `crc_lut_sliced` is
as fast at half the size.

| Preset | Kernel |
| --- | --- |
| `x86_pclmul`, `x86_avx2` | PCLMULQDQ folding for every CRC from 16 B; for CRC-32C, SSE4.2 `crc32` |
| `x86_vpclmul`, `x86_avxvnni` | As above, plus 256-bit VPCLMULQDQ folding from 256 B |
| `x86_avx512` | As above, the long loop in 512-bit vectors (not benchmarked: tested in Intel SDE only) |
| `aarch64_crc`, `thumb2_crc` | CRC32 instructions for CRC-32 and CRC-32C |
| `aarch64_pmull` | CRC32 instructions, plus PMULL folding for every other CRC from 16 B (little-endian only) |
| `riscv_zbc` | Zbc `clmul` folding from 16 B, on little-endian RV64 |
| all others | Portable sliced or braided loop |

The CRC32 instructions serve any CRC with the CRC-32 or CRC-32C polynomial and reflected input in a `std::uint32_t`
register, whatever its initial value and final XOR.

### Performance

- Each step from `crc_lut_none` to `crc_lut_sliced` is 1.5× to several times faster.
- A compile-time CRC of a large input is slow, because constant evaluation runs the byte-table loop at best.

Full figures, including Cortex-M flash and RAM footprints, are in [performance.md](performance.md#crc).

### Limitations

- Only widths 1 to 64, and only polynomials with an x^0 term.
- Initial values are in the direct form, never the "augmented" one.
