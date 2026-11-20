# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

A C++23 library of non-cryptographic checksums and hashes, from microcontrollers to servers: CRC of any width,
the Internet checksum, Adler-32, Fletcher, ZFS fletcher4, xxHash and MurmurHash3. Every algorithm runs at compile time
and at run time with the same result on every platform, never allocates, and uses CPU instructions where they pay off.

## Why this one

- **Plain C++23.** Header-first API, one namespace, no macros to configure; the run-time loops are compiled once in a
  small static library.
- **`constexpr` everywhere.** A checksum computed in a `static_assert` equals the run-time one, on every CPU and byte
  order. Named CRC engines and their tables are constants in read-only data.
- **No allocation, no global state, no exceptions or RTTI.** Builds with `-fno-exceptions -fno-rtti` on bare-metal
  toolchains; a CRC-32 engine takes 32 bytes without a table or 8 KiB of tables for full speed, your choice.
- **Microcontroller-friendly linking.** Each algorithm, strategy and CPU preset is its own object, so a static link
  keeps only what the program calls.
- **Fast.** CPU kernels per [instruction-set preset](#instruction-set-presets): carry-less multiplication, CRC
  instructions and vector units on x86-64, Arm and RISC-V, and the DSP instructions of Cortex-M4/M7/M33. CRC-32 runs at
  about 75 000 MB/s on a laptop CPU, on par with Intel ISA-L. Optional run-time dispatch picks the preset for the
  running CPU.
- **Checked against the references.** Values match zlib, OpenZFS, xxHash and SMHasher. CI tests natively on x86-64
  and AArch64, with MSVC on Windows, in QEMU for AArch64, 32-bit Arm, RISC-V and big-endian AArch64, and bare metal on
  Cortex-M3/M4/M7/M33 and Cortex-A9 in QEMU system mode.

Out of scope: cryptographic hashes and message authentication (SHA, SipHash), and a C API.

## Algorithms

| Algorithm | Use it for | Header | Guide |
| --- | --- | --- | --- |
| CRC, widths 1-64, 112 named variants | Error detection in protocols, files, flash images | `checksum/crc.hpp` | [crc.md](doc/crc.md) |
| Internet checksum (RFC 1071) | IPv4, ICMP, IGMP, UDP, TCP | `checksum/internet.hpp` | [internet.md](doc/internet.md) |
| Adler-32 (RFC 1950) | zlib, PNG | `checksum/adler32.hpp` | [adler32.md](doc/adler32.md) |
| Fletcher-16, -32, -64 | Protocols that specify them | `checksum/fletcher.hpp` | [fletcher.md](doc/fletcher.md) |
| fletcher4 | ZFS | `checksum/fletcher4.hpp` | [fletcher4.md](doc/fletcher4.md) |
| XXH3-64, XXH3-128 | Hash tables, deduplication, cache keys | `checksum/xxh3.hpp` | [xxh3.md](doc/xxh3.md) |
| XXH32, XXH64 | Hash tables, cache keys, legacy formats | `checksum/xxhash.hpp` | [xxhash.md](doc/xxhash.md) |
| MurmurHash3 x86_32, x64_128 | Compatibility with existing MurmurHash3 values | `checksum/murmur3.hpp` | [murmur3.md](doc/murmur3.md) |

Measured speed and code size on x86-64, Cortex-A72, Cortex-M4 and Cortex-M33: [doc/performance.md](doc/performance.md).

## Quick start

```cpp
#include <checksum/crc.hpp>
#include <checksum/xxh3.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::crc_compute<checksum::crc32>("123456789"sv) == 0xCBF43926);

// Explicit preset and strategy, on any span or range of bytes: AVX2 where this build has it (x86-64), and
// crc_lut_sliced, which trades 8 KiB of tables for speed.
std::vector<std::byte> data = read_file("firmware.bin");
constexpr auto avx2 = checksum::isa_available(checksum::isa::x86_avx2) ? checksum::isa::x86_avx2 : checksum::isa::generic;
if(checksum::detect_isa().contains(avx2)) {  // a preset is a promise: check the CPU, or know it
  std::uint32_t crc = checksum::crc_compute<checksum::crc32, checksum::crc_lut_sliced, avx2>(data);
}

// Run-time dispatch: the best preset of this build for the running CPU, chosen once.
static checksum::xxh3_dispatch_engine<64> const xxh3;
checksum::isa chosen = xxh3.preset();
std::uint64_t hash = xxh3.compute(data);
```

The quick start of each guide shows these three ways for its algorithm.

Every algorithm has the same shape:

- **One-shot:** `<algorithm>_compute<[Width,] Isa>(data[, seed])`, e.g. `adler32_compute(data)`,
  `xxh3_compute<128>(data, seed)`, `crc_compute<checksum::crc32>(data)`.
- **Engines:** `<algorithm>_engine<[Width,] Isa>` (CRC: `crc_engine<Register, Strategy, Isa>::create(parameters)` or
  `crc_engine_for<Parameters>`) holds what does not change during a computation: CRC parameters and tables, the seed of
  a hash. The running state is a separate value: `initial()`, `update(state, data)`, `finalize(state)`,
  `compute(data)`, the preset in `preset()`, and for CRC, Internet, Adler-32, Fletcher and fletcher4
  `resume(checksum)`, the state of a message with a stored checksum. The `engine` and `resumable_engine` concepts
  (`checksum/engine.hpp`) describe them.
- **Accumulator:** `checksum::accumulator running{engine}` keeps an engine pointer and the state together:
  `running.update(header).update(payload).value()`. It works with every engine, dispatch engines included.

[examples/](examples) has small complete programs for every algorithm: files read in chunks, a MODBUS frame, an IPv4
header and UDP datagram, a zlib trailer, a ZFS block, a Bloom filter, a seeded hash table, and a custom CRC strategy
that drives the CRC peripheral of an STM32.

## Getting it

With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):

```cmake
CPMAddPackage("gh:jmacheta/checksum@2.0.0")
target_link_libraries(my_app PRIVATE checksum::checksum)
```

`FetchContent` from `https://github.com/jmacheta/checksum.git`, or a copy of the repository with
`add_subdirectory(checksum)`, works the same way. Tests and examples build only when checksum is the top-level project
(`CHECKSUM_TESTS`, `CHECKSUM_EXAMPLES`).

`cmake --install` puts the library, headers and a CMake package in the prefix, for `find_package(checksum CONFIG)` and
`checksum::checksum`; `-DCHECKSUM_INSTALL=OFF` turns the install rules off (they are on by default, also as a subproject).

The headers are meant to be included as system (MSVC: external) headers, so that your warning flags do not apply to them:
CMake does that for the installed package's imported target, and for a subproject with `FetchContent_Declare(... SYSTEM)`
or `add_subdirectory(checksum SYSTEM)`.

Compile with `-ffunction-sections -fdata-sections` and link with `-Wl,--gc-sections`, so the binary keeps only the
algorithms you use.

Requirements: CMake 3.25, and GCC 14 or newer, Clang 18 or newer (with libstdc++ 14 or libc++ 18), or MSVC for x64 or
ARM64 (CI tests x64).

## Instruction-set presets

Every algorithm takes an instruction-set preset, `checksum::isa` (`checksum/isa.hpp`), as its last explicit template
parameter. The default `isa::generic` runs on every CPU of the architecture: the portable loops plus what the
architecture guarantees, SSE2 on x86-64 and NEON on little-endian AArch64. The other presets add CPU
extensions; each includes the ones before it on its line:

| Architecture | Presets |
| --- | --- |
| x86-64 | `x86_pclmul` → `x86_avx2` → `x86_vpclmul` → `x86_avxvnni` |
| AArch64 | `aarch64_crc` → `aarch64_pmull` |
| 32-bit Arm (GCC, Clang) | `thumb2` → `thumb2_dsp` → `thumb2_neon` → `thumb2_crc` |
| RISC-V RV64 (GCC, Clang) | `riscv_zbc` and `riscv_v`, independent |

GCC, Clang, MSVC and clang-cl build the same x86-64 and AArch64 kernels, GCC and Clang the same 32-bit Arm kernels. Not
supported yet: AVX-512, Arm Helium (MVE) and SVE2.

- **A preset is a compile-time choice and a promise.** `internet_compute<checksum::isa::x86_avx2>(data)` runs the AVX2
  kernel; on a CPU without AVX2 that is undefined behavior (an illegal instruction). Compiler flags such as
  `-march=native` select no kernel, every preset gives the same result, and constant evaluation runs the portable code.
- **What a build has:** `isa_available(preset)` is `constexpr`; naming a preset it rejects fails the engine's `requires`-clause, also in
  an `if constexpr` branch outside a template. Portable code names its preset through a constant:
  `constexpr auto avx2 = isa_available(isa::x86_avx2) ? isa::x86_avx2 : isa::generic;`. On
  32-bit Arm the presets need a Thumb-2 build; `thumb2_dsp` also needs the DSP extension, and `thumb2_neon` and
  `thumb2_crc` an A-profile ARMv7 or later build with a hardware floating-point ABI (Clang needs more, see Platform
  differences below). A Cortex-M0 build has only `isa::generic`. Each guide lists which preset enables which kernel.

To choose at run time, make a dispatch engine. It detects the CPU when it is made and runs the best preset of this build
from then on; each call costs one or two cycles to switch to the chosen loop.

```cpp
static checksum::internet_dispatch_engine const internet;  // detects the CPU at program start
static auto const crc = checksum::crc_dispatch_engine<std::uint32_t, checksum::crc_lut_sliced>::create(checksum::crc32);

std::uint16_t const sum = internet.compute(data);
std::uint32_t const value = crc->compute(data);
```

- Every algorithm has one, with the same members as its static engine: `internet_dispatch_engine`,
  `adler32_dispatch_engine`, `fletcher_dispatch_engine<Width>`, `fletcher4_dispatch_engine` (constructor
  `explicit engine(isa_set cpu = detect_isa())`), `xxh3_dispatch_engine<Width>`, `xxhash_dispatch_engine<Width>`,
  `murmur3_dispatch_engine<Width>` (constructor `explicit engine(seed = 0, isa_set cpu = detect_isa())`), and
  `crc_dispatch_engine<Register, Strategy>::create(parameters, isa_set cpu = detect_isa())`, which returns
  `std::optional` and keeps one table for every preset. `preset()` names the chosen preset.
- `detect_isa()` asks the CPU or the operating system on every call: the CPU identification instruction on x86-64,
  the Linux auxiliary vector and system calls on Arm and RISC-V, `IsProcessorFeaturePresent` on Windows ARM64,
  `elf_aux_info` on FreeBSD and `sysctlbyname` on Apple AArch64. It also reports what the compiler flags already require
  of the CPU (`-mcpu=cortex-m4` gives `thumb2_dsp`, `-march=armv8-a+crc` gives `aarch64_crc`); without an
  operating-system feature query, bare metal included, that is all it reports. Make engines once, not per message: a
  global engine detects the CPU during program start-up, a function-local `static` one on its first use.
- An explicit `isa_set` chooses instead, e.g. from a configuration: `internet_dispatch_engine(checksum::isa::x86_avx2)`
  runs the AVX2 variant, falls back one rung at a time where the build lacks it, and runs `isa::generic` on another
  architecture. A preset converts to the set of its line up to it; sets combine with `|`. On RISC-V, where both lines
  are built without target options, an engine picks only a preset with a kernel: `riscv_zbc` for CRC, `riscv_v` for the
  Internet checksum and XXH3, `generic` for the others.
- xxHash, MurmurHash3 and the CRC strategies `crc_lut_none`, `crc_lut_nibble` and `crc_lut_byte` have one loop, built
  for `isa::generic`, which every preset runs: their dispatch engines choose `generic` on every architecture and
  compiler.
- **constexpr:** `detect_isa()` runs only at run time, so a default-constructed engine cannot be `constexpr`. Give an
  explicit set: `constexpr checksum::internet_dispatch_engine engine(checksum::isa_set{})` (generic) or a preset. This
  is deliberate: a constant-evaluable detection would silently make every `static` engine generic.
- Programs that make no dispatch engine link none of this: `detect_isa()` is a source of its own.

Platform differences, by design:

- **Clang on 32-bit Arm** builds the `thumb2` presets only in a Thumb-2 build (`-mthumb`), and `thumb2_neon` and
  `thumb2_crc` only with the build's own NEON (`-mfpu=neon` or later); GCC adds both itself.
- **Arm builds without DSP** (`-mcpu=cortex-m33+nodsp`, `cortex-m55+nodsp`) build and detect `thumb2`, not
  `thumb2_dsp`, also on a core that has DSP.
- **Bare-metal Cortex-A with the MMU off:** all data memory is then Strongly-ordered, where unaligned accesses fault.
  GCC and Clang emit unaligned accesses for ARMv7-A, in this library and elsewhere (newlib's `memcpy` too), so
  `-mno-unaligned-access` is not enough: map memory as Normal before running C++ code, as `tests/bare_metal/startup.S`
  does.
- **MSVC** compiles the presets without target options. Where an algorithm has no kernel for a preset (the Internet
  checksum below `x86_avx2`, for example), a dispatch engine still chooses and
  reports that preset, which runs the same portable code as `isa::generic`. On RISC-V, the other build without target
  options, engines choose only presets with a kernel (above).

## Migrating from 1.x

- **CPU kernels are no longer chosen by compiler flags.** A 1.x build with `-march=native` or `-mavx2` ran the
  kernels; in 2.0 the default `isa::generic` runs only the architecture baseline (SSE2, AArch64 NEON). Name a preset
  (`internet_compute<checksum::isa::x86_avx2>(data)`) or use a dispatch engine to get them back.
- **The CMake option `CHECKSUM_ACCELERATION` is gone**; `isa::generic` is the portable choice.
- **One engine model for every algorithm.** The free update and finalize functions are gone: make the engine
  (`internet_engine<Isa>{}`, `xxh3_engine<64, Isa>{seed}`) and call its `update` and `finalize`. A seed is given to
  the engine, no longer stored in the state. Continuing from a stored checksum is
  `engine.update(engine.resume(checksum), data)`. The aliases of each width are gone: write `fletcher_compute<16>`,
  `xxhash_compute<32>`, `xxh3_state<64>`. The CRC accumulator became `checksum::accumulator`, and
  `crc_dispatch_engine` names its strategy.
- **Explicit template arguments moved:** the preset is now the first template parameter (after the width, where there is
  one). Results are unchanged everywhere.

## Contributing

```sh
cmake --workflow --preset native-gcc   # configure, build and run the tests
```

[AGENTS.md](AGENTS.md) is the map for contributors, human or not: code layout, commands, and the design documents in
[doc/design](doc/design).

## License

[MIT](LICENSE.txt)
