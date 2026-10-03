# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

A C++23 library for checksums, built for everything from microcontrollers to servers. It currently provides CRCs:
any width from 1 to 64 bits, 112 named parameter sets, compile-time or run-time computation, and CPU acceleration
where the compiler flags allow it.

```cpp
#include <checksum/crc_catalog.hpp>
using namespace std::literals;

static_assert(checksum::crc_engine_for<checksum::crc32>.compute("123456789"sv) == 0xCBF43926);

constexpr auto const &crc32 = checksum::crc_engine_for<checksum::crc32, checksum::crc_lut_sliced>;
std::uint32_t value = crc32.compute(std::span(buffer));
```

## Design goals

- **Modern C++:** C++23, `constexpr` everywhere it can be, concepts and ranges in the API.
- **Memory-efficient:** no dynamic allocation and no global state. You choose how much table memory to spend on speed,
  from a few bytes up, and pay nothing for what you do not use.
- **Fast:** portable loops that compete on their own, plus CRC and carry-less multiplication instructions on x86-64,
  Arm and RISC-V.
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
- [examples/crc](examples/crc): compile-time CRC, a file checksum, a custom CRC, run-time parameters, a MODBUS frame
  and a strategy driving an MCU CRC peripheral.

## Contributing

Build and run the tests with a CMake workflow preset:

```sh
cmake --workflow --preset native-gcc
```

Design rules, code style, tooling and testing are described in [doc/design](doc/design); [AGENTS.md](AGENTS.md)
indexes them.
