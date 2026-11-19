# CRC user guide

`checksum` computes cyclic redundancy checks of any width from 1 to 64 bits, over byte- and bit-granular messages,
at compile time or at run time. Everything is in two headers:

- `checksum/crc.hpp`: the parameter model, the engines, the strategies;
- `checksum/crc_catalog.hpp`: 112 named parameter sets (`checksum::crc32`, `checksum::crc16_modbus`, ...).

Requirements: C++23, GCC ≥ 14 with libstdc++, or Clang ≥ 18 with libstdc++ ≥ 14 or libc++ ≥ 18. The library
throws no exceptions, allocates no memory, uses no RTTI and has no global state; it builds with
`-fno-exceptions -fno-rtti` and on bare-metal toolchains.

## 1. Adding the library

```cmake
add_subdirectory(checksum)            # or CPMAddPackage / FetchContent
target_link_libraries(app PRIVATE checksum::checksum)
```

| CMake option | Default | Effect |
| --- | --- | --- |
| `CHECKSUM_TESTS` | top-level project | Unit tests and negative compile tests (downloads GoogleTest). |
| `CHECKSUM_BENCHMARKS` | `OFF` | Benchmarks (downloads Google Benchmark and zlib); on in the `native-*-bench` presets. |
| `CHECKSUM_EXAMPLES` | top-level project | Builds `examples/`. |

Compile and link the application with `-ffunction-sections -fdata-sections` and `-Wl,--gc-sections`: the library
compiles the run-time loops of every strategy and register type, and the linker then keeps only the ones the
program uses. Unused catalog entries cost nothing either way.

## 2. Quick start

```cpp
#include <checksum/crc_catalog.hpp>
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

`examples/crc` has complete programs: compile-time CRC, a file checksum, a custom CRC, run-time parameters, a
MODBUS frame and a strategy driving an MCU CRC peripheral.

## 3. Parameter model

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

`initial_value` and `final_xor` are written unreflected, as published in CRC catalogs. The initial value is the
register content before the first message bit (catalogs call this the "direct" form).

**Polynomials.** `crc_polynomial` stores one 64-bit value in Koopman notation: bit i is the coefficient of
x^(i + 1), the x^0 term is implicit and the highest set bit gives the width. CRC-16/CCITT
(x^16 + x^12 + x^5 + 1) is `0x8810`; the constructor is implicit, so a plain number initializes the field. The
normal form found in data sheets converts with `crc_polynomial::create(0x1021, 16)`, which returns `std::nullopt`
if the width is above 64, the value does not fit the width or the x^0 term is missing. `width()` and
`normal_form()` convert back.

**Validity.** A parameter set is valid for an engine if the width is 1..64 and fits the engine's register, and
`initial_value` and `final_xor` are below 2^width. `crc_engine::create` returns `std::nullopt` otherwise;
`crc_engine_for` fails to compile (the diagnostic names `std::bad_optional_access`).

## 4. Catalog

`checksum/crc_catalog.hpp` defines every CRC of width ≤ 64 from the "Catalogue of parametrised CRC algorithms"
as `inline constexpr crc_parameters`. A name is the catalog name in lower case with `CRC-` written `crc` and the
other `/` and `-` mapped to `_`: `crc32_iso_hdlc`, `crc16_ibm_3740`, `crc15_can`. Each entry's comment lists its
polynomial and the catalog's other names for it.

Aliases are references to the same object: `crc32` (CRC-32/ISO-HDLC), `crc32c` (CRC-32/ISCSI),
`crc16_ccitt_false` (CRC-16/IBM-3740) and `crc16_x25` (CRC-16/IBM-SDLC). There is no bare `crc64`: the catalog's
CRC-64 is ECMA-182, while common usage often means CRC-64/XZ.

## 5. Engines

`crc_engine<Register, Strategy = crc_lut_none, isa Isa = isa::generic>` holds a strategy's table and the prepared
parameters. It is immutable, so one engine serves any number of computations; the running CRC is a separate
`state_type` value. The preset `Isa` selects the run-time loop of the built-in strategies; it is orthogonal to the
table strategy.

| Member | Meaning |
| --- | --- |
| `static create(parameters)` | Validates the parameters and builds the table; `std::optional`. |
| `initial()` | State of the empty message. |
| `update(state, data)` | Folds bytes into a state. |
| `update(state, data, bit_length)` | Folds the first `bit_length` bits of `data` (section 5.2). |
| `finalize(state)` | Turns a state into the CRC. |
| `compute(data)`, `compute(data, bit_length)` | `finalize(update(initial(), ...))`. |

`crc_engine_for<Parameters, Strategy = crc_lut_none, isa Isa = isa::generic>` is the engine built at compile time,
with the smallest register (`crc_register_for<width>`). It is a constant in read-only data, so a program that uses
it contains no table-building code. Equal parameter values name the same object:
`crc_engine_for<crc_parameters{.polynomial = 0x83}>` is `crc_engine_for<checksum::crc8_smbus>`.

`crc_engine<Register, Strategy, Isa>::create(parameters)` builds an engine at run time (or in a `constexpr`
context). `Register` is `std::uint8_t`, `std::uint16_t`, `std::uint32_t` or `std::uint64_t`. A wider register than
the CRC needs gives the same results, so one engine type can serve run-time parameter sets of any width up to 64.

Splitting a message anywhere gives the same CRC: `finalize(update(update(initial(), a), b)) == compute(a ++ b)`.
`crc_accumulator<Register, Strategy, Isa>` wraps that: it holds a pointer to the engine and a state, and offers
`update` (chainable), `value()` (does not change the state), `state()` and `reset()`. The engine must outlive the
accumulator; constructing one from a temporary engine does not compile.

### 5.1 Input

Every function taking data accepts `std::span<std::byte const>` and any input range of `std::byte`, `char`,
`unsigned char`, `signed char` or `char8_t`. Contiguous ranges are passed on as one span; other ranges (lists,
views, generators) are copied through a 64-byte buffer on the stack.

Arrays of `char` and `char8_t` are rejected at compile time, since a string literal's `'\0'` would be included:
pass text as `std::string_view` and other `char` buffers wrapped in `std::span`. Ranges of `volatile` elements are
rejected as well; copy memory-mapped buffers first.

### 5.2 Bit-granular messages

`update(state, data, bit_length)` folds the first `bit_length / 8` bytes, then `bit_length % 8` bits of the next
byte. Bits are taken in the message order of the CRC: from bit 0 up if the input is reflected, from bit 7 down
otherwise; the other bits of that byte are ignored. For example, the 83-bit message of a CAN frame:

```cpp
constexpr auto const &can = checksum::crc_engine_for<checksum::crc15_can>;
std::uint16_t crc = can.compute(bits, 83);   // bits: std::array<std::byte, 11>
```

### 5.3 Preconditions

A state passed to `update` or `finalize` must be below 2^width, and `bit_length` must not exceed
`8 * data.size()`. Both are checked with `assert`. With `NDEBUG` the state is masked and the bit length clamped,
so a violation gives a wrong CRC but never undefined behavior. Without `NDEBUG`, a failed `assert` during constant evaluation is a
compile error.

## 6. Strategies

A strategy decides how whole bytes are folded into the register and what the engine stores for it. Bit tails,
reflection, the initial value and the final XOR are handled by the engine, so all strategies give identical
results.

| Strategy | Engine size (8 / 16 / 32 / 64-bit register) | Method |
| --- | --- | --- |
| `crc_lut_none` (default) | 11 / 16 / 32 / 64 B | Bitwise, no table. Smallest code and data. |
| `crc_lut_nibble` | 27 / 48 / 96 / 192 B | 16-entry table, two lookups per byte. |
| `crc_lut_byte` | 267 / 528 / 1056 / 2112 B | 256-entry table, one lookup per byte. |
| `crc_lut_sliced` | 2.2 / 4.2 / 8.2 / 16.2 KiB | Slicing-by-8: eight 256-entry tables, one 8-byte word per step. CPU kernels for the engine's preset (section 7). |
| `crc_lut_braided` | 4.2 / 8.2 / 16.2 / 32.2 KiB | Slicing-by-8 plus 8 braid tables: five interleaved streams of 8-byte words from 128 B on. CPU kernels for the engine's preset (section 7). |

Sizes are `sizeof(crc_engine)` on a 64-bit host. The sliced and braided engines include 160 B of folding constants
for the CPU kernels. Large engines belong in static storage (`crc_engine_for`, or a `static`/global object), not on
an MCU stack.

Choosing:

- **Flash- or RAM-constrained MCU:** `crc_lut_none` or `crc_lut_nibble`.
- **MCU with spare memory:** `crc_lut_byte`, or `crc_lut_sliced` for the best speed (section 8). A copy of the
  engine in RAM is about 1.5× faster than one in flash on a Cortex-M4.
- **Desktop and server CPUs:** `crc_lut_sliced`. With a CPU preset it is the fastest strategy everywhere. With
  `isa::generic`, `crc_lut_braided` is about twice as fast from 128 B on, on out-of-order cores.

### 6.1 Custom strategies

Any type modeling `crc_strategy_for<Strategy, Register>` is a strategy:

```cpp
struct my_strategy {
  template <class Register> using table_type = /* semiregular: what update() needs */;
  template <class Register> static std::optional<table_type<Register>> make_table(checksum::crc_parameters const &);
  template <class Register> static Register update(table_type<Register> const &, Register state, std::span<std::byte const>);
};
```

`make_table` runs once per engine and returns `std::nullopt` for parameters the strategy cannot handle (for example
a peripheral that supports only some polynomials). `update` receives the public state: below 2^width, reflected if
the input is reflected. It must return the state after the bytes. A strategy needs to support only the registers it
names. Strategies without `constexpr` members work with `crc_engine::create` at run time only.

The engine's `Isa` preset is passed to the built-in strategies. A custom strategy keeps its interface: the engine
calls `Strategy::update<Register>(...)` and ignores the preset for it, unless it provides
`Strategy::template update<Register, Isa>(...)`. A hardware strategy does not need to name a preset.
`examples/crc/hardware_strategy.cpp` drives the CRC unit of STM32 parts this way.

## 7. CPU acceleration

`crc_lut_sliced` and `crc_lut_braided` use CPU instructions selected by the engine's instruction-set preset. The
preset is a template parameter of the engine, orthogonal to the table strategy: strategies are memory/speed
choices, presets are CPU choices. The engine passes the preset to the built-in strategies; a custom strategy keeps
its interface (section 6.1).

Compiler flags such as `-march=native`, `-mpclmul` or `-march=armv8-a+crc` select no CRC kernel; choose a preset.

| Preset | Kernel |
| --- | --- |
| `isa::generic` | portable sliced or braided loop on every architecture. SSE2 and AArch64 NEON have no CRC instructions. |
| `isa::x86_pclmul`, `isa::x86_avx2`, `isa::x86_vpclmul`, `isa::x86_avxvnni` on x86-64 | PCLMULQDQ folding for every parameter set from 16 B. CRC-32C also uses the SSE4.2 `crc32` instruction: inputs of at least 25 B fold first and reduce with two `crc32` instructions. |
| `isa::x86_vpclmul`, `isa::x86_avxvnni` on x86-64 | additionally the 256-bit VPCLMULQDQ loop from 256 B. |
| `isa::aarch64_crc` | CRC32/CRC32C instructions for CRC-32 and CRC-32C. |
| `isa::aarch64_pmull` | CRC32/CRC32C instructions, plus PMULL folding for the other parameter sets. Little-endian AArch64 only; big-endian keeps the CRC instructions. |
| `isa::thumb2_crc` | CRC32/CRC32C instructions (32-bit Arm, AArch32 with the CRC extension, GCC). |
| `isa::riscv_zbc` | Zbc `clmul`/`clmulh` folding (RV64, little-endian). The inline assembly enables Zbc locally, so the build itself needs no Zbc. |
| `isa::thumb2`, `isa::thumb2_neon`, `isa::thumb2_dsp`, `isa::riscv_v` | portable sliced or braided loops compiled for the preset. |

**Choosing a preset.** `crc_engine_for<checksum::crc32c, checksum::crc_lut_sliced, checksum::isa::x86_pclmul>.compute(data)`
runs the x86 PCLMULQDQ kernel whatever the build's compiler flags. The library compiles the loop once for every
preset that `isa_available()` accepts on the target architecture and compiler, each into its own object, so a static
link pulls in only the presets a program names. A preset without a kernel still runs the portable loop compiled for
its instruction set. Naming a preset that `isa_available()` rejects is a compile error, not a link error. Presets
give bit-identical results, and constant evaluation runs the same portable code for every preset. The library does
not check the CPU: calling a preset on a CPU without its instructions is undefined (illegal instruction). There is
no run-time detection yet; a later run-time layer is planned.

Notes:

- The CRC-32 and CRC-32C instructions apply to any parameter set with that polynomial and reflected input in a
  `std::uint32_t` register, whatever its initial value and final XOR.
- `crc_lut_braided` runs the `crc_lut_sliced` code wherever a kernel applies, so with a kernel the
  two strategies perform the same and `crc_lut_sliced` is half the size.
- x86-64-v3 (AVX2) does not include PCLMULQDQ; choose `isa::x86_pclmul` or a higher preset.
- Big-endian AArch64 uses the CRC32 instructions but not PMULL.
- `crc_lut_none`, `crc_lut_nibble` and `crc_lut_byte` run their portable loop for every preset.
- Constant evaluation always runs the portable loops; results are identical.
- MSVC x64 uses the same x86 kernels per preset. MSVC ARM64 has no CRC kernel and runs the portable loops.

The design and the measurements behind each kernel are in [design/acceleration.md](design/acceleration.md).

## 8. Performance guidelines

Measured figures for every strategy and platform are in [performance.md](performance.md#crc). The "accelerated"
column there is the `isa::x86_vpclmul` preset.

- **Choose the strategy by memory first** (section 6). Each step from `crc_lut_none` to `crc_lut_sliced` is 1.5× to
  several times faster than the one below it, and costs more table memory.
- **Choose the preset for the target CPU.** `crc_lut_sliced` and `crc_lut_braided` use the kernels of section 7 only
  for the engine's preset. The kernels take over at these sizes:
  - carry-less folding (PCLMULQDQ, PMULL, Zbc): every parameter set, from 16 B;
  - VPCLMULQDQ (`isa::x86_vpclmul` and up): 256 B per iteration, from 256 B;
  - the Arm CRC instructions (`isa::aarch64_crc`, `isa::thumb2_crc`): CRC-32 and CRC-32C;
  - SSE4.2 `crc32`: CRC-32C, folding from 25 B in `isa::x86_pclmul` and up.

  On x86-64 CRC-32 then runs at about 75 000 MB/s from 4 KiB, against about 3 000 MB/s for the portable sliced loop.
- **With `isa::generic` on a desktop CPU**, `crc_lut_braided` is about twice as fast as `crc_lut_sliced` from
  128 B on. On in-order cores such as Cortex-M it is slower and twice the size.
- **On a microcontroller, keep the engine in RAM** if it fits. Table lookups from flash wait for the memory: a copy
  of the engine in RAM is about 1.5× faster on a Cortex-M4, and `crc_lut_sliced` 1.2-1.4× faster on the nRF54L15,
  whose instruction cache holds no data. Engines take kilobytes, so use a static object, not the stack.
- **A CRC peripheral** driven by a custom strategy (section 6.1) runs as fast as `crc_lut_sliced` in RAM on long
  messages on an STM32L4A6, with no table. Its per-call setup, and the software bit reversal of a reflected CRC,
  make it slower on short messages.
- **Compile-time CRCs of large inputs are slow:** constant evaluation runs the byte-table loop at best.

## 9. Limitations

- **Widths 1..64** only, and only polynomials with an x^0 term.
- **Initial values are direct**: the register content before the first message bit, never the "augmented" form.
- **No run-time CPU detection.** The preset is chosen at compile time; calling a preset on a CPU without its
  instructions is undefined (illegal instruction). A binary that names `x86_vpclmul` does not run on older CPUs.
- **Not thread-safe.** A `const` engine can be shared between threads; an accumulator cannot.
