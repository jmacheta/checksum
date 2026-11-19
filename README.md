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
| XXH3-64, XXH3-128, XXH32, XXH64 | Hash tables, deduplication, cache keys | `checksum/xxh3.hpp`, `checksum/xxhash.hpp` | [xxhash.md](doc/xxhash.md) |
| MurmurHash3 x86_32, x64_128 | Compatibility with existing MurmurHash3 values | `checksum/murmur3.hpp` | [murmur3.md](doc/murmur3.md) |

Measured speed and code size on x86-64, Cortex-A72, Cortex-M4 and Cortex-M33: [doc/performance.md](doc/performance.md).

## Quick start

```cpp
#include <checksum/crc.hpp>
#include <checksum/xxh3.hpp>
using namespace std::literals;

// Compile time.
static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

// Run time, on any span or range of bytes; crc_lut_sliced trades 8 KiB of tables for speed.
std::vector<std::byte> data = read_file("firmware.bin");
std::uint32_t crc = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>.compute(data);

// The best CPU kernel for the running CPU, detected once at program start.
static checksum::xxh3_dispatch_engine<64> const xxh3;
std::uint64_t hash = xxh3.compute(data);
```

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
- **What a build has:** `isa_available(preset)` is `constexpr`; naming a preset it rejects is a compile error. On
  32-bit Arm the presets need a Thumb-2 build; `thumb2_dsp` also needs the DSP extension, and `thumb2_neon` and
  `thumb2_crc` an A-profile ARMv7 or later build with a hardware floating-point ABI. Clang also needs a build in Thumb
  state (`-mthumb`) and, for `thumb2_neon` and `thumb2_crc`, the build's own NEON (`-mfpu=neon` or later). A Cortex-M0
  build has only `isa::generic`. Each guide lists which preset enables which kernel.

To choose at run time, make a dispatch engine. It detects the CPU when it is made and runs the best preset of this build
from then on; each call costs one or two cycles to switch to the chosen loop.

```cpp
static checksum::internet_dispatch_engine const internet;  // detects the CPU at program start
static auto const crc = checksum::crc_dispatch_engine<std::uint32_t>::create(checksum::crc32);

std::uint16_t const sum = internet.compute(data);
std::uint32_t const value = crc->compute(data);
```

- Every algorithm has one: `internet_dispatch_engine`, `adler32_dispatch_engine`, `fletcher_dispatch_engine<Width>`,
  `fletcher4_dispatch_engine`, `xxh3_dispatch_engine<Width>`, `xxhash_dispatch_engine<Width>`,
  `murmur3_dispatch_engine<Width>` (constructor `explicit engine(isa_set cpu = detect_isa())`), and
  `crc_dispatch_engine<Register, Strategy = crc_lut_sliced>::create(parameters, isa_set cpu = detect_isa())`, which
  returns `std::optional` and keeps one table for every preset. `compute` and `update` match the free functions, and
  the XXH3 and CRC engines also have `finalize`; the others finish with the algorithm's free `*_finalize`. `preset()`
  names the chosen preset.
- `detect_isa()` asks the CPU or the operating system on every call: the CPU identification instruction on x86-64,
  the Linux auxiliary vector and system calls on Arm and RISC-V, `IsProcessorFeaturePresent` on Windows ARM64,
  `elf_aux_info` on FreeBSD and `sysctlbyname` on Apple AArch64. It also reports what the compiler flags already require
  of the CPU (`-mcpu=cortex-m4` gives `thumb2_dsp`, `-march=armv8-a+crc` gives `aarch64_crc`); without an
  operating-system feature query, bare metal included, that is all it reports. Make engines once, not per message: a
  `static` or global engine detects the CPU during program start-up.
- An explicit `isa_set` chooses instead, e.g. from a configuration: `internet_dispatch_engine(checksum::isa::x86_avx2)`
  runs the AVX2 variant, falls back one rung at a time where the build lacks it, and runs `isa::generic` on another
  architecture. A preset converts to the set of its line up to it; sets combine with `|`. On RISC-V, where both lines
  are built, CRC prefers `riscv_zbc` and the others `riscv_v`.
- **constexpr:** `detect_isa()` runs only at run time, so a default-constructed engine cannot be `constexpr`. Give an
  explicit set: `constexpr checksum::internet_dispatch_engine engine(checksum::isa_set{})` (generic) or a preset. This
  is deliberate: a constant-evaluable detection would silently make every `static` engine generic.
- Programs that make no dispatch engine link none of this: `detect_isa()` is a source of its own.

## Migrating from 1.x

- **CPU kernels are no longer chosen by compiler flags.** A 1.x build with `-march=native` or `-mavx2` ran the
  kernels; in 2.0 the default `isa::generic` runs only the architecture baseline (SSE2, AArch64 NEON). Name a preset
  (`internet_compute<checksum::isa::x86_avx2>(data)`) or use a dispatch engine to get them back.
- **The CMake option `CHECKSUM_ACCELERATION` is gone**; `isa::generic` is the portable choice.
- **Explicit template arguments moved:** the preset is now the first template parameter (after the width, where there is
  one), so a call such as `internet_update<std::uint16_t>(checksum, data)` drops the explicit type. Results are
  unchanged everywhere.

## Contributing

```sh
cmake --workflow --preset native-gcc   # configure, build and run the tests
```

[AGENTS.md](AGENTS.md) is the map for contributors, human or not: code layout, commands, and the design documents in
[doc/design](doc/design).

## License

[MIT](LICENSE.txt)
