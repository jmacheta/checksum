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
| `CHECKSUM_ACCELERATION` | `ON` | Lets `crc_lut_sliced` and `crc_lut_braided` use the CPU instructions that the compiler flags enable (section 7). `OFF` keeps only the portable loops. |
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

`crc_engine<Register, Strategy>` holds a strategy's table and the prepared parameters. It is immutable, so one
engine serves any number of computations; the running CRC is a separate `state_type` value.

| Member | Meaning |
| --- | --- |
| `static create(parameters)` | Validates the parameters and builds the table; `std::optional`. |
| `initial()` | State of the empty message. |
| `update(state, data)` | Folds bytes into a state. |
| `update(state, data, bit_length)` | Folds the first `bit_length` bits of `data` (section 5.2). |
| `finalize(state)` | Turns a state into the CRC. |
| `compute(data)`, `compute(data, bit_length)` | `finalize(update(initial(), ...))`. |

`crc_engine_for<Parameters, Strategy>` is the engine built at compile time, with the smallest register
(`crc_register_for<width>`). It is a constant in read-only data, so a program that uses it contains no
table-building code. Equal parameter values name the same object: `crc_engine_for<crc_parameters{.polynomial = 0x83}>`
is `crc_engine_for<checksum::crc8_smbus>`.

`crc_engine<Register, Strategy>::create(parameters)` builds an engine at run time (or in a `constexpr` context).
`Register` is `std::uint8_t`, `std::uint16_t`, `std::uint32_t` or `std::uint64_t`. A wider register than the CRC
needs gives the same results, so one engine type can serve run-time parameter sets of any width up to 64.

Splitting a message anywhere gives the same CRC: `finalize(update(update(initial(), a), b)) == compute(a ++ b)`.
`crc_accumulator` wraps that: it holds a pointer to the engine and a state, and offers `update` (chainable),
`value()` (does not change the state), `state()` and `reset()`. The engine must outlive the accumulator;
constructing one from a temporary engine does not compile.

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
| `crc_lut_sliced` | 2.2 / 4.2 / 8.2 / 16.2 KiB | Slicing-by-8: eight 256-entry tables, one 8-byte word per step. CPU acceleration (section 7). |
| `crc_lut_braided` | 4.2 / 8.2 / 16.2 / 32.2 KiB | Slicing-by-8 plus 8 braid tables: five interleaved streams of 8-byte words from 128 B on. CPU acceleration (section 7). |

Sizes are `sizeof(crc_engine)` on a 64-bit host. The sliced and braided engines include 160 B of folding constants
for the CPU kernels. Large engines belong in static storage (`crc_engine_for`, or a `static`/global object), not on
an MCU stack.

Choosing:

- **Flash- or RAM-constrained MCU:** `crc_lut_none` or `crc_lut_nibble`.
- **MCU with spare memory:** `crc_lut_byte`, or `crc_lut_sliced` for the best speed (section 8). A copy of the
  engine in RAM is about 1.5× faster than one in flash on a Cortex-M4.
- **Desktop and server CPUs:** `crc_lut_sliced`. With acceleration it is the fastest strategy everywhere. Without it,
  `crc_lut_braided` is about twice as fast from 128 B on, on out-of-order cores.

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
`examples/crc/hardware_strategy.cpp` drives the CRC unit of STM32 parts this way.

## 7. CPU acceleration

`crc_lut_sliced` and `crc_lut_braided` use CPU instructions when the compiler flags enable them. The choice is
made by the preprocessor in the library's sources, from the macros the compiler predefines for the flags. There is
no run-time CPU detection, so the library must be compiled with the flags of the target CPU:

| Target | Flags (examples) | What runs |
| --- | --- | --- |
| x86-64 with PCLMULQDQ | `-mpclmul -msse4.1` | Carry-less multiplication folding for every parameter set, inputs ≥ 16 B. |
| x86-64 with VPCLMULQDQ and AVX2 | `-march=icelake-client`, `alderlake`, `znver3`, `native` | Inputs ≥ 256 B also fold 256 B per iteration. |
| x86-64 with SSE4.2 | `-msse4.2`, `-march=x86-64-v2` | The `crc32` instruction for CRC-32C; with PCLMULQDQ also available, CRC-32C inputs ≥ 25 B fold instead. |
| AArch64 / AArch32 with the CRC extension | `-march=armv8-a+crc` | The CRC32 instructions for CRC-32 and CRC-32C. |
| Little-endian AArch64 with PMULL | `-march=armv8-a+crc+crypto` | Folding for every other parameter set; with `+sha3` (Neoverse V1/V2, Apple M1 and later) each fold uses one `EOR3`. |
| RV64, little-endian, with Zbc | `-march=rv64gc_zbc` | Folding with `clmul` / `clmulh`. |

Notes:

- The CRC-32 and CRC-32C instructions apply to any parameter set with that polynomial and reflected input in a
  `std::uint32_t` register, whatever its initial value and final XOR.
- `crc_lut_braided` runs the `crc_lut_sliced` code wherever acceleration applies, so in accelerated builds the two
  strategies perform the same and `crc_lut_sliced` is half the size.
- x86-64-v3 (AVX2) does not include PCLMULQDQ; add `-mpclmul`.
- Big-endian AArch64 uses the CRC32 instructions but not PMULL.
- Constant evaluation always runs the portable loops; results are identical.
- `CHECKSUM_ACCELERATION=OFF` disables all of it.

The design and the measurements behind each kernel are in [design/acceleration.md](design/acceleration.md).

## 8. Performance guidelines

Measured figures for every strategy and platform are in [performance.md](performance.md#crc).

- **Choose the strategy by memory first** (section 6). Each step from `crc_lut_none` to `crc_lut_sliced` is 1.5× to
  several times faster than the one below it, and costs more table memory.
- **Compile for the target CPU.** `crc_lut_sliced` and `crc_lut_braided` use the instructions of section 7 only if
  the compiler flags enable them. The kernels take over at these sizes:
  - carry-less folding (PCLMULQDQ, PMULL, Zbc): every parameter set, from 16 B;
  - VPCLMULQDQ with AVX2: 256 B per iteration, from 256 B;
  - the CRC instructions of Arm (`+crc`): CRC-32 and CRC-32C;
  - SSE4.2 `crc32`: CRC-32C, folding from 25 B when PCLMULQDQ is also enabled.

  On x86-64 CRC-32 then runs at about 75 000 MB/s from 4 KiB, against about 3 000 MB/s for the portable sliced loop.
- **Without acceleration on a desktop CPU**, `crc_lut_braided` is about twice as fast as `crc_lut_sliced` from
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
- **No run-time CPU detection.** Acceleration is fixed by the compiler flags of the library build; a binary built
  with `-march=native` does not run on older CPUs.
- **Not thread-safe.** A `const` engine can be shared between threads; an accumulator cannot.
