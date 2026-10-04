# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

A C++23 library for checksums, built for everything from microcontrollers to servers. It currently provides:

- **CRCs:** any width from 1 to 64 bits, 112 named parameter sets;
- **the Internet checksum** (RFC 1071) of IPv4, ICMP, UDP and TCP;
- **Fletcher-16, Fletcher-32, Fletcher-64 and Adler-32** (RFC 1950);
- **fletcher4**, the checksum of ZFS;
- **XXH32, XXH64, XXH3-64 and XXH3-128** with a seed;
- **MurmurHash3** x86_32 and x64_128 with a seed.

All compute at compile time or at run time, most with CPU acceleration where the compiler flags allow it.

```cpp
#include <checksum/crc_catalog.hpp>
using namespace std::literals;

static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

constexpr auto const &crc32 = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>;
std::uint32_t value = crc32.compute(std::span(buffer));

#include <checksum/internet.hpp>
std::uint16_t ip_checksum = checksum::internet_compute(std::span(ipv4_header));
```

## Design goals

- **Modern C++:** C++23, `constexpr` everywhere it can be, concepts and ranges in the API.
- **Memory-efficient:** no dynamic allocation and no global state. You choose how much table memory to spend on speed,
  from a few bytes up, and pay nothing for what you do not use.
- **Fast:** portable loops that compete on their own, plus CRC, carry-less multiplication and vector instructions on
  x86-64, Arm and RISC-V.
- **Portable and embedded-friendly:** no exceptions, RTTI or OS calls; works on bare-metal toolchains and any
  endianness, with identical results everywhere.

## Getting it

With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):

```cmake
CPMAddPackage("gh:jmacheta/checksum@1.0.0")
target_link_libraries(my_app PRIVATE checksum::checksum)
```

Or download [release 1.0.0](https://github.com/jmacheta/checksum/releases/tag/v1.0.0) and use
`add_subdirectory(checksum)`.

Requires GCC 14 or newer, or Clang 18 or newer with libstdc++ 14 or libc++ 18.

CPU acceleration follows your compiler flags (for example `-march=native` or `-march=armv8-a+crc`); set
`CHECKSUM_ACCELERATION=OFF` to keep only the portable code. Link with `-Wl,--gc-sections` and compile with
`-ffunction-sections -fdata-sections`, so the binary keeps only what it uses.

## Documentation

- [CRC user guide](doc/crc.md): parameters, the catalog, engines, strategies and their memory cost, CPU
  acceleration, measured performance and limitations.
- [Internet checksum user guide](doc/internet.md): the API, splitting messages, CPU acceleration, measured performance
  and limitations.
- [Fletcher user guide](doc/fletcher.md) and [Adler-32 user guide](doc/adler32.md): the API, CPU acceleration,
  measured performance and limitations.
- [fletcher4 user guide](doc/fletcher4.md): the definition and its relation to ZFS, the API, CPU acceleration,
  measured performance and limitations.
- [xxHash user guide](doc/xxhash.md): XXH32, XXH64, XXH3-64 and XXH3-128, seeds, streaming cost, CPU acceleration,
  measured performance and limitations.
- [MurmurHash3 user guide](doc/murmur3.md): the API, seeds, byte order, measured performance and limitations.
- [examples/crc](examples/crc): compile-time CRC, a file checksum, a custom CRC, run-time parameters, a MODBUS frame
  and a strategy driving an MCU CRC peripheral.
- [examples/internet](examples/internet): an IPv4 header, a UDP datagram with its pseudo-header and an incremental
  update after a TTL decrement.
- [examples/fletcher](examples/fletcher) and [examples/adler32](examples/adler32): compile-time checksums, a file
  checksum, a frame check, a zlib trailer and extending a stored Adler-32.
- [examples/fletcher4](examples/fletcher4): verifying a ZFS block and a file checksum.
- [examples/xxhash](examples/xxhash) and [examples/xxh3](examples/xxh3): compile-time IDs, file hashes, a cache key,
  block deduplication and a seeded hash table.
- [examples/murmur3](examples/murmur3): command dispatch, a Bloom filter and record deduplication.

## Contributing

Build and run the tests with a CMake workflow preset:

```sh
cmake --workflow --preset native-gcc
```

Design rules, code style, tooling and testing are described in [doc/design](doc/design); [AGENTS.md](AGENTS.md)
indexes them.
