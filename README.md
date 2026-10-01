# checksum

[![CI](https://github.com/jmacheta/checksum/actions/workflows/ci.yml/badge.svg)](https://github.com/jmacheta/checksum/actions/workflows/ci.yml)

Checksum-computation library. Currently covers CRC only (`checksum::crc`);
other algorithms may be added later under the `checksum` namespace.

CI builds and tests every push and pull request on Linux (GCC, Clang,
ASan+UBSan), with best-effort macOS and Windows jobs; see
[`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## Build & test

Configure, build and test via CMake presets:

    cmake --workflow --preset native-gcc
    cmake --workflow --preset native-clang
    cmake --workflow --preset native-asan

Or step by step:

    cmake --preset native-gcc
    cmake --build --preset native-gcc-debug
    ctest --preset native-gcc-debug

## Layout

- `src/include/checksum` - public headers (installed API)
- `src/include_private/checksum_private` - internal headers
- `src/crc` - CRC sources
- `tests/unit_tests` - Google Test unit tests
- `doc/specification/crc.md` - authoritative CRC specification (WIP)
