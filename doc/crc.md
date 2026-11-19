# CRC user guide

Cyclic redundancy checks of any width from 1 to 64 bits, over byte- and bit-granular messages, at compile time or at run
time. One header, `checksum/crc.hpp`, has the parameter model, the engines and the strategies, and includes
`checksum/crc_catalog.hpp` with 112 named parameter sets.

## 1. Quick start

```cpp
#include <checksum/crc.hpp>
using namespace std::literals;

// Compile time: the engine and its table are constants in read-only data.
static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

// Run time, with a faster strategy.
constexpr auto const &crc32 = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>;
std::uint32_t value = crc32.compute(std::span(buffer));

// Incremental.
checksum::crc_accumulator accumulator{crc32};
accumulator.update(header).update(payload);
std::uint32_t frame_crc = accumulator.value();

// Parameters known only at run time.
auto engine = checksum::crc_polynomial::create(normal_form, width).and_then([](checksum::crc_polynomial polynomial) {
  return checksum::crc_engine<std::uint64_t, checksum::crc_lut_byte>::create({.polynomial = polynomial});
});
```

`examples/crc` has complete programs: compile-time CRC, a file checksum, a custom CRC, run-time parameters, a MODBUS
frame and a strategy driving an MCU CRC peripheral.

## 2. Definition

A CRC is described by `crc_parameters`, a plain aggregate usable as a template argument:

```cpp
struct crc_parameters {
  crc_polynomial polynomial;        // generator polynomial; its degree is the CRC width
  std::uint64_t initial_value = 0;  // register value before the first message bit
  bool reflect_input = false;       // message bytes are fed least significant bit first
  bool reflect_output = false;      // the result is reflected before the final XOR
  std::uint64_t final_xor = 0;      // XORed into the result
};

inline constexpr checksum::crc_parameters my_crc{
    .polynomial = 0xC002,           // x^16 + x^15 + x^2 + 1
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
};
```

`initial_value` and `final_xor` are written unreflected, as published in CRC catalogs; the initial value is the
"direct" form, the register content before the first message bit.

**Polynomials.** `crc_polynomial` stores one 64-bit value in Koopman notation: bit i is the coefficient of x^(i + 1),
the x^0 term is implicit and the highest set bit gives the width. CRC-16/CCITT (x^16 + x^12 + x^5 + 1) is `0x8810`; the
constructor is implicit, so a plain number initializes the field. The normal form of data sheets converts with
`crc_polynomial::create(0x1021, 16)`, which returns `std::nullopt` if the width is above 64, the value does not fit the
width or the x^0 term is missing. `width()` and `normal_form()` convert back.

**Validity.** A parameter set is valid for an engine if the width is 1..64 and fits the engine's register, and
`initial_value` and `final_xor` are below 2^width. `crc_engine::create` returns `std::nullopt` otherwise;
`crc_engine_for` fails to compile (the diagnostic names `std::bad_optional_access`).

**Catalog.** `checksum/crc_catalog.hpp` defines every CRC of width ≤ 64 from the "Catalogue of parametrised CRC
algorithms" as `inline constexpr crc_parameters`. A name is the catalog name in lower case with `CRC-` written `crc`
and other `/` and `-` mapped to `_`: `crc32_iso_hdlc`, `crc16_ibm_3740`, `crc15_can`. Each entry's comment lists its
polynomial and other names. Aliases refer to the same object: `crc32` (CRC-32/ISO-HDLC), `crc32c` (CRC-32/ISCSI),
`crc16_ccitt_false` (CRC-16/IBM-3740) and `crc16_x25` (CRC-16/IBM-SDLC). There is no bare `crc64`: the catalog's
CRC-64 is ECMA-182, while common usage often means CRC-64/XZ.

## 3. API

### 3.1 Engines

`crc_engine<Register, Strategy = crc_lut_none, isa Isa = isa::generic>` holds a strategy's table and the prepared
parameters. It is immutable, so one engine serves any number of computations; the running CRC is a separate
`state_type` value. `Register` is `std::uint8_t`, `std::uint16_t`, `std::uint32_t` or `std::uint64_t`; a wider register
than the CRC needs gives the same results. `Isa` picks the CPU kernel (section 5), independent of the table strategy.

| Member | Meaning |
| --- | --- |
| `static create(parameters)` | Validates the parameters and builds the table; `std::optional`. Works in `constexpr` contexts. |
| `initial()` | State of the empty message. |
| `update(state, data)` | Folds bytes into a state. |
| `update(state, data, bit_length)` | Folds the first `bit_length` bits of `data` (section 4). |
| `finalize(state)` | Turns a state into the CRC. |
| `compute(data)`, `compute(data, bit_length)` | `finalize(update(initial(), ...))`. |

`crc_engine_for<Parameters, Strategy = crc_lut_none, isa Isa = isa::generic>` is the engine built at compile time with
the smallest register (`crc_register_for<width>`). It is a constant in read-only data, so the program contains no
table-building code. Equal parameter values name the same object: `crc_engine_for<crc_parameters{.polynomial = 0x83}>`
is `crc_engine_for<checksum::crc8_smbus>`.

`crc_accumulator<Register, Strategy, Isa>` holds a pointer to an engine and a state, and offers `update` (chainable),
`value()` (does not change the state), `state()` and `reset()`. The engine must outlive it; constructing one from a
temporary engine does not compile.

`crc_dispatch_engine<Register, Strategy = crc_lut_sliced>::create(parameters, isa_set cpu = detect_isa())` chooses the
preset at run time (with `riscv_zbc` first on RISC-V), keeps one table for every preset, and has the members above plus
`preset()`. It is not accepted by `crc_accumulator`. See
[Instruction-set presets](../README.md#instruction-set-presets).

### 3.2 Strategies

A strategy decides how whole bytes are folded and what the engine stores. Bit tails, reflection, the initial value and
the final XOR are handled by the engine, so all strategies give identical results.

| Strategy | Engine size (8 / 16 / 32 / 64-bit register) | Method |
| --- | --- | --- |
| `crc_lut_none` (default) | 11 / 16 / 32 / 64 B | Bitwise, no table. Smallest code and data. |
| `crc_lut_nibble` | 27 / 48 / 96 / 192 B | 16-entry table, two lookups per byte. |
| `crc_lut_byte` | 267 / 528 / 1056 / 2112 B | 256-entry table, one lookup per byte. |
| `crc_lut_sliced` | 2.2 / 4.2 / 8.2 / 16.2 KiB | Slicing-by-8: eight 256-entry tables, one 8-byte word per step. CPU kernels. |
| `crc_lut_braided` | 4.2 / 8.2 / 16.2 / 32.2 KiB | Slicing-by-8 plus 8 braid tables: five interleaved streams from 128 B. CPU kernels. |

Sizes are `sizeof(crc_engine)` on a 64-bit host; sliced and braided include 160 B of folding constants. Large engines
belong in static storage (`crc_engine_for`, or a `static` object), not on an MCU stack.

- **Flash- or RAM-constrained MCU:** `crc_lut_none` or `crc_lut_nibble`.
- **MCU with spare memory:** `crc_lut_byte`, or `crc_lut_sliced` for the best speed.
- **Desktop and server CPUs:** `crc_lut_sliced` with a CPU preset. With `isa::generic`, `crc_lut_braided` is about
  twice as fast from 128 B on out-of-order cores.

### 3.3 Custom strategies

Any type modeling `crc_strategy_for<Strategy, Register>` is a strategy:

```cpp
struct my_strategy {
  template <class Register> using table_type = /* semiregular: what update() needs */;
  template <class Register> static std::optional<table_type<Register>> make_table(checksum::crc_parameters const &);
  template <class Register> static Register update(table_type<Register> const &, Register state, std::span<std::byte const>);
};
```

`make_table` runs once per engine and returns `std::nullopt` for parameters the strategy cannot handle (for example a
peripheral that supports only some polynomials). `update` receives the public state, below 2^width and reflected if the
input is reflected, and returns the state after the bytes. A strategy needs to support only the registers it names;
one without `constexpr` members works with `crc_engine::create` at run time only. The engine ignores its `Isa` for a
custom strategy unless the strategy provides `template update<Register, Isa>(...)`.
`examples/crc/hardware_strategy.cpp` drives the CRC unit of STM32 parts this way.

## 4. Behavior

- **Splits:** `finalize(update(update(initial(), a), b)) == compute(a ++ b)` for any split.
- **Input:** `std::span<std::byte const>` or any input range of `std::byte`, `char`, `unsigned char`, `signed char` or
  `char8_t`; non-contiguous ranges are copied through a 64-byte stack buffer. Arrays of `char` and `char8_t` are
  rejected, since a string literal's `'\0'` would be included: pass text as `std::string_view`. Ranges of `volatile`
  elements are rejected; copy memory-mapped buffers first.
- **Bit-granular messages:** `update(state, data, bit_length)` folds `bit_length / 8` bytes, then `bit_length % 8` bits
  of the next byte, taken in the CRC's message order: from bit 0 up if the input is reflected, from bit 7 down
  otherwise. The 83-bit message of a CAN frame:

  ```cpp
  std::uint16_t crc = checksum::crc_engine_for<checksum::crc15_can>.compute(bits, 83);  // std::array<std::byte, 11>
  ```

- **Preconditions:** a state passed to `update` or `finalize` must be below 2^width, and `bit_length` at most
  `8 * data.size()`. Both are checked with `assert`; with `NDEBUG` the state is masked and the length clamped, so a
  violation gives a wrong CRC but never undefined behavior. A failed `assert` during constant evaluation is a compile
  error.

## 5. CPU acceleration

`crc_lut_sliced` and `crc_lut_braided` use CPU instructions selected by the engine's preset; the other strategies run
the portable loop for every preset. Presets are explained in
[Instruction-set presets](../README.md#instruction-set-presets).

| Preset | Kernel |
| --- | --- |
| `isa::x86_pclmul`, `x86_avx2` | PCLMULQDQ folding for every parameter set from 16 B; CRC-32C also uses SSE4.2 `crc32` from 25 B. |
| `isa::x86_vpclmul`, `x86_avxvnni` | As above, plus the 256-bit VPCLMULQDQ loop from 256 B. |
| `isa::aarch64_crc`, `isa::thumb2_crc` | CRC32/CRC32C instructions for CRC-32 and CRC-32C. |
| `isa::aarch64_pmull` | CRC32/CRC32C instructions, plus PMULL folding for other parameter sets (little-endian only). |
| `isa::riscv_zbc` | Zbc `clmul` folding (RV64, little-endian); the build itself needs no Zbc. |
| every other preset, `isa::generic` included | Portable sliced or braided loop. |

- The CRC instructions apply to any parameter set with the CRC-32 or CRC-32C polynomial and reflected input in a
  `std::uint32_t` register, whatever its initial value and final XOR.
- With a kernel `crc_lut_braided` runs the sliced code, so `crc_lut_sliced` is as fast and half the size.
- x86-64-v3 (AVX2) does not imply PCLMULQDQ; the x86 presets all include it.
- MSVC x64 uses the same x86 kernels; MSVC ARM64 has none.

Design and decisions per kernel: [design/acceleration.md](design/acceleration.md#crc).

### 5.1 Choosing the preset at run time

A dispatch engine asks the CPU once, when it is made, and from then on runs the best variant this build has for it:

```cpp
#include <checksum/crc.hpp>

// Made once, during program start-up: detect_isa() queries the CPU or the operating system.
static auto const crc32 = checksum::crc_dispatch_engine<std::uint32_t>::create(checksum::crc32).value();

std::uint32_t frame_check(std::span<std::byte const> frame) { return crc32.compute(frame); }
```

`crc32.preset()` names the chosen preset. An explicit preset chooses instead, e.g. from a configuration:
`crc_dispatch_engine<std::uint32_t>::create(checksum::crc32, checksum::isa::x86_avx2)` runs that variant, falling back
one rung at a time where this build lacks it; it does not check the CPU. A call costs one switch over the presets (one
or two cycles) on top of the variant. Detection does not run in constant expressions: there, call
`crc_engine_for<…>.compute` or give the engine an explicit preset. Details and the list of engines: [Instruction-set
presets](../README.md#instruction-set-presets).

## 6. Performance guidelines

Measured figures: [performance.md](performance.md#crc).

- **Choose the strategy by memory first** (section 3.2): each step from `crc_lut_none` to `crc_lut_sliced` is 1.5× to
  several times faster and costs more table memory.
- **Then the preset for the CPU.** On x86-64 CRC-32 runs about 75 000 MB/s from 4 KiB with `isa::x86_vpclmul`,
  against about 3 000 MB/s for the portable sliced loop.
- **On a microcontroller, keep the engine in RAM** if it fits: table lookups from flash wait for the memory, about
  1.5× slower on a Cortex-M4 and 1.2-1.4× for `crc_lut_sliced` on the nRF54L15. On in-order cores `crc_lut_braided` is
  slower than `crc_lut_sliced` and twice the size.
- **A CRC peripheral** driven by a custom strategy matches `crc_lut_sliced` in RAM on long messages on an STM32L4A6,
  with no table; its per-call setup makes it slower on short ones.
- **Compile-time CRCs of large inputs are slow:** constant evaluation runs the byte-table loop at best.

## 7. Limitations

- **Widths 1..64** only, and only polynomials with an x^0 term.
- **Initial values are direct**, never the "augmented" form.
- **A preset in a template argument is not checked:** on a CPU without its instructions it is undefined behavior.
  `crc_dispatch_engine` checks the CPU.
- **Not thread-safe:** a `const` engine can be shared between threads; an accumulator cannot.
