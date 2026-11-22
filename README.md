# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

A C++23 library of fast non-cryptographic checksums and hashes, for everything from a Cortex-M0 to a server. One API
covers CRC, the Internet checksum, Adler-32, Fletcher, fletcher4, xxHash, MurmurHash3 and FNV-1a, at compile time
and at run time.

## Features

- **Eight algorithm families:** CRC of any width with all 112 catalogued CRCs built in, the Internet checksum,
  Adler-32, Fletcher-16/32/64, ZFS fletcher4, xxHash (XXH32, XXH64, XXH3), MurmurHash3 and FNV-1a (32, 64).
- **Verified values:** CI checks results against reference vectors generated with zlib, OpenZFS, xxHash and
  SMHasher, and against the CRC catalogue's check values, on every unit-test target, big-endian AArch64 included.
- **`constexpr` everywhere:** any checksum can be computed in a `static_assert`, with the same result as at run time.
- **Embedded-friendly:** no heap, no global state, no exceptions or RTTI needed. A table-free CRC-32 costs about
  100 bytes of code and a 32-byte engine on Cortex-M4.
- **Fast:** kernels for PCLMULQDQ, AVX2, AVX-VNNI, Arm CRC32, PMULL, NEON, the Cortex-M DSP instructions, and RISC-V
  Zbc and V. On long x86-64 inputs it is on par with xxHash, OpenZFS and Intel ISA-L, and faster than
  zlib-ng, Linux and DPDK.
- **Run-time dispatch:** name the instruction set at compile time, or let the library pick the best for the running CPU.
- **Portable:** GCC 14+, Clang 18+, MSVC and clang-cl, on x86-64, AArch64, 32-bit Arm, RISC-V (RV32 and RV64) and bare metal.
- **Tested in CI:** unit tests run natively on Linux, Windows (x64, ARM64) and macOS (Apple silicon), and in QEMU for
  32-bit Arm, RISC-V and big-endian AArch64. On bare-metal Cortex-M3/M4/M7/M33 and A9, QEMU runs the benchmarks' checks of `detect_isa()` and
  of every preset and dispatch engine against the generic one.

## Quick start

```cpp
#include <checksum/crc.hpp>
#include <checksum/xxh3.hpp>
using namespace std::literals;

// At compile time.
static_assert(checksum::crc_compute<checksum::crc32>("123456789"sv) == 0xCBF43926);

// The default CRC engine is bitwise and table-free: small, but about 160 MB/s on x86-64.
std::uint32_t crc = checksum::crc_compute<checksum::crc32>(data);

// 8 KiB of tables and the best kernel for the running CPU: up to about 86 000 MB/s.
using sliced_dispatch = checksum::crc_dispatch_engine<std::uint32_t, checksum::crc_lut_sliced>;
static auto const fast_crc = sliced_dispatch::create(checksum::crc32).value();
std::uint32_t crc_fast = fast_crc.compute(data);

// Hashes, and data that arrives in pieces.
std::uint64_t hash = checksum::xxh3_compute<64>(data);
checksum::accumulator running{fast_crc};
std::uint32_t frame_crc = running.update(header).update(payload).value();
```

Every algorithm has the same parts: a one-shot function, an engine with `initial`, `update` and `finalize`, and a
dispatch engine. [examples/](examples) has complete programs, such as a MODBUS frame, a UDP checksum, a zlib
trailer, a ZFS block and an STM32 CRC peripheral driver.

## Adding it to your project

```cmake
CPMAddPackage("gh:jmacheta/checksum@2.0.0")
target_link_libraries(my_app PRIVATE checksum::checksum)
```

`FetchContent`, `add_subdirectory` and `find_package` work too. checksum is a static library; without CMake, compile
`src/**/*.cpp` with `-std=c++23` and the include paths `src/include` and `src/include_private`.
A few toolchains need more:

- Clang on 32-bit Arm builds the `thumb2` presets only with `-mthumb`, and the NEON ones only with a NEON `-mfpu`.
- Clang 18 on RISC-V builds `riscv_v` only with `-march=rv64gcv`.
- Bare-metal Cortex-A code must map memory as Normal before C++ runs, since unaligned accesses fault with the MMU off.

## Hardware acceleration

Every function takes an instruction-set preset as its last template argument. The default, `isa::generic`, runs on
every CPU of the architecture; the others add CPU extensions, each including the ones before it on its line:

| Architecture | Presets |
| --- | --- |
| x86-64 | `x86_pclmul` → `x86_avx2` → `x86_vpclmul` → `x86_avxvnni` |
| AArch64 | `aarch64_crc` → `aarch64_pmull` |
| 32-bit Arm | `thumb2` → `thumb2_dsp` → `thumb2_neon` → `thumb2_crc` |
| RISC-V RV64 | `riscv_zbc` and `riscv_v`, independent of each other |

Name a preset when you know the target CPU, or use a dispatch engine, which picks one at run time. Compiler flags do not
select kernels, and every preset gives the same result. Running a preset on a CPU that lacks it is undefined behavior.
Each guide's presets table lists which preset enables which kernel.

## Documentation

| Algorithm | Header | Guide |
| --- | --- | --- |
| CRC | `checksum/crc.hpp` | [doc/crc.md](doc/crc.md) |
| Internet checksum | `checksum/internet.hpp` | [doc/internet.md](doc/internet.md) |
| Adler-32 | `checksum/adler32.hpp` | [doc/adler32.md](doc/adler32.md) |
| Fletcher-16, -32, -64 | `checksum/fletcher.hpp` | [doc/fletcher.md](doc/fletcher.md) |
| fletcher4 | `checksum/fletcher4.hpp` | [doc/fletcher4.md](doc/fletcher4.md) |
| XXH32, XXH64 | `checksum/xxhash.hpp` | [doc/xxhash.md](doc/xxhash.md) |
| XXH3-64, XXH3-128 | `checksum/xxh3.hpp` | [doc/xxh3.md](doc/xxh3.md) |
| MurmurHash3 | `checksum/murmur3.hpp` | [doc/murmur3.md](doc/murmur3.md) |
| FNV-1a 32, 64 | `checksum/fnv1a.hpp` | [doc/fnv1a.md](doc/fnv1a.md) |

[doc/performance.md](doc/performance.md) has measured speed, code size and comparisons on x86-64, Cortex-A72, Cortex-M4
and Cortex-M33.

## Migrating from 1.x

- **Compiler flags no longer select kernels.** In 1.x, `-march=native` or `-mavx2` enabled the kernels. In 2.0, name a
  preset, such as `internet_compute<checksum::isa::x86_avx2>(data)`, or use a dispatch engine. The
  `CHECKSUM_ACCELERATION` CMake option is gone.
- **Every algorithm uses engines.** The free `update` and `finalize` functions are gone: create the engine, such as
  `internet_engine<Isa>{}` or `xxh3_engine<64, Isa>{seed}`, and call its members. The seed now belongs to the engine,
  not to the state, and `engine.update(engine.resume(checksum), data)` continues from a stored checksum.
- **Some names changed.** The per-width aliases are gone, so write `fletcher_compute<16>`, `xxhash_compute<32>` and
  `xxh3_state<64>`. The CRC accumulator is now `checksum::accumulator`, and `crc_dispatch_engine` takes its strategy
  explicitly.
- **The preset is the last explicit template argument,** after the width or, for CRC, after the strategy. All results
  are unchanged.

## License

[MIT](LICENSE.txt)
