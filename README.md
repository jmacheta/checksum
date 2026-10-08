# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

A C++23 library of checksums and fast hashes, from microcontrollers to servers. Every algorithm works at
compile time and at run time, gives the same result on every platform, and never allocates.

```cpp
#include <checksum/crc_catalog.hpp>
#include <checksum/xxh3.hpp>
using namespace std::literals;

// At compile time...
static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

// ...or at run time, on any span or range of bytes.
std::vector<std::byte> data = read_file("firmware.bin");
std::uint32_t crc = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>.compute(data);
std::uint64_t hash = checksum::xxh3_64_compute(data);
```

## What is in it

| You need | Use | Header | Guide |
| --- | --- | --- | --- |
| Error detection in a protocol, file format or flash image | **CRC**, any width 1-64, 112 named variants (CRC-32, CRC-16/MODBUS, CRC-15/CAN, ...) | `checksum/crc_catalog.hpp` | [crc.md](doc/crc.md) |
| IPv4, ICMP, UDP or TCP checksums | **Internet checksum** (RFC 1071) | `checksum/internet.hpp` | [internet.md](doc/internet.md) |
| zlib / PNG compatibility | **Adler-32** (RFC 1950) | `checksum/adler32.hpp` | [adler32.md](doc/adler32.md) |
| A cheap check where a protocol asks for it | **Fletcher-16, -32, -64** | `checksum/fletcher.hpp` | [fletcher.md](doc/fletcher.md) |
| ZFS compatibility | **fletcher4** | `checksum/fletcher4.hpp` | [fletcher4.md](doc/fletcher4.md) |
| Hash tables, deduplication, cache keys | **XXH3-64, XXH3-128**, or **XXH32, XXH64** | `checksum/xxh3.hpp`, `checksum/xxhash.hpp` | [xxhash.md](doc/xxhash.md) |
| Compatibility with existing MurmurHash3 values | **MurmurHash3** x86_32 and x64_128 | `checksum/murmur3.hpp` | [murmur3.md](doc/murmur3.md) |

## Why this one

- **Fast where it counts.** Portable loops that hold their own, plus CPU instructions where your compiler flags
  allow them: carry-less multiplication, CRC instructions and vector units on x86-64, Arm and RISC-V, and the DSP
  instructions of Cortex-M4/M7/M33. CRC-32 runs at about 75 000 MB/s on a laptop CPU, on par with Intel ISA-L.
  [doc/performance.md](doc/performance.md) has the measurements, from servers down to 64 MHz microcontrollers.
- **You choose the memory.** A CRC-32 engine takes 32 bytes without a table, or 8 KiB of tables for full
  speed. Nothing allocates, there is no global state, and the linker drops what you do not call.
- **Embedded-friendly.** No exceptions, RTTI or OS calls; builds with `-fno-exceptions -fno-rtti` for bare-metal
  targets, on little- and big-endian CPUs.
- **Checked against the references.** Values match zlib, OpenZFS, xxHash and SMHasher. CI runs the tests natively
  and in QEMU for AArch64, 32-bit Arm, RISC-V and big-endian AArch64.

## Getting it

With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):

```cmake
CPMAddPackage("gh:jmacheta/checksum@1.0.0")
target_link_libraries(my_app PRIVATE checksum::checksum)
```

Or download [release 1.0.0](https://github.com/jmacheta/checksum/releases/tag/v1.0.0), put it next to your code and
use `add_subdirectory(checksum)` with the same `target_link_libraries`.

You need GCC 14 or newer, or Clang 18 or newer (with libstdc++ 14 or libc++ 18 or newer).

Two build tips:

- CPU acceleration follows your compiler flags, for example `-march=native` or `-mcpu=cortex-a72+crc`; there is no
  run-time CPU detection. Set `CHECKSUM_ACCELERATION=OFF` to keep only the portable code.
- Compile with `-ffunction-sections -fdata-sections` and link with `-Wl,--gc-sections`, so the binary keeps only
  the algorithms you use.

## Examples

Every algorithm has small complete programs in [examples/](examples): compile-time values, checksums of files read in
chunks, a MODBUS frame, an IPv4 header and UDP datagram, a zlib trailer, a ZFS block, a Bloom filter, a seeded hash
table and a custom CRC strategy that drives the CRC peripheral of an STM32.

## Contributing

Build and run the tests with a CMake workflow preset:

```sh
cmake --workflow --preset native-gcc
```

Design rules, code style, tooling and testing are described in [doc/design](doc/design), indexed by
[AGENTS.md](AGENTS.md).

## License

[MIT](LICENSE.txt)
