# AGENTS.md

A C++23 library of non-cryptographic checksums and hashes (CRC, Internet checksum, Adler-32, Fletcher, fletcher4,
xxHash, MurmurHash3): `constexpr`, no allocation, CPU kernels selected by instruction-set presets. Read
[README.md](README.md) first for the user-facing model.

## Where things are

| Path | Contents |
| --- | --- |
| `src/include/checksum/` | Public headers, one per algorithm (+ `crc_catalog.hpp`), `isa.hpp` (presets, `isa_set`), `byte_range.hpp` |
| `src/include_private/checksum_private/` | `<algorithm>_loops.hpp` (portable loops), `<algorithm>_arch.hpp` (selects the directory), `presets.hpp` and `target_begin/end.hpp` (target options) |
| `src/include_private/checksum_private/{x86_64,arm,riscv64,generic}/` | Architecture kernels, chosen with `if constexpr` on the preset |
| `src/<algorithm>/` | Run-time loops, one source per preset (`sum_loop_x86_avx2.cpp`, `lut_sliced_aarch64_crc.cpp`) |
| `src/isa/detect.cpp` | `detect_isa()`, the only CPU query |
| `src/CMakeLists.txt` | Every library source, `detect.cpp` included, listed by hand in `target_sources(checksum PRIVATE ...)` (no glob); a new source goes there, not in the top-level `CMakeLists.txt` |
| `tests/unit_tests/` | Google Test, `ut_<algorithm>[_<area>].cpp` listed in its `CMakeLists.txt`, one executable `checksum_ut`; helpers in `include/` and `tests/include/` |
| `tests/compile_fail/` | Negative compile tests |
| `tests/bare_metal/` | `detect_isa()` and every dispatch engine on bare-metal Arm in QEMU system mode (`bare-cortex-*` presets) |
| `tests/benchmarks/` | Google Benchmark, `checksum_bench_<algorithm>` (`checksum_bench` is CRC) |
| `examples/<algorithm>/` | Small complete programs, built but not run as tests |

## Commands

Configure presets are in `CMakePresets.json` and `cmake/*.json`; build and test presets add `-debug` (and `-release` for
the plain native builds), binaries go to `build/<preset>/`.

```sh
cmake --workflow --preset native-gcc            # configure + build + test, the usual check
cmake --preset native-gcc && cmake --build --preset native-gcc-debug
ctest --preset native-gcc-debug -R internet     # only the tests of the algorithm you touched
cmake --workflow --preset cross-aarch64         # QEMU; also cross-arm, cross-arm-crc32, cross-arm-clang, cross-arm-crc32-clang, cross-riscv64, cross-aarch64be
```

- During development build and run only what you touched (`ctest -R <algorithm>`); run the full matrix of
  [testing.md](doc/design/testing.md#what-to-run) only before finishing a change to shared code.
- Benchmarks (`native-gcc-bench`, `native-clang-bench`: configure and build presets only) and the QEMU matrix are slow;
  run them rarely, filtered (`--benchmark_filter=`), and only when the change needs them.
- Before finishing: `clang-format -i` on changed C++, `cmake-format -i` on changed CMake, `cspell --config .cspell.yml`
  on changed files, and no new clang-tidy findings (configured in `.clangd`). Details in
  [build_and_tooling.md](doc/design/build_and_tooling.md).
- Do not commit unless asked.

## Documents

| File | Read it when |
| --- | --- |
| [doc/design/principles.md](doc/design/principles.md) | Adding or changing any API or algorithm: goals, structure, what is out of scope. |
| [doc/design/code_style.md](doc/design/code_style.md) | Writing or reviewing code: layout, naming, comments, errors and preconditions, API shape. |
| [doc/design/build_and_tooling.md](doc/design/build_and_tooling.md) | Touching CMake, presets, dependencies or CI; the formatting and lint checks. |
| [doc/design/testing.md](doc/design/testing.md) | Writing tests, or deciding which presets to run. |
| [doc/design/acceleration.md](doc/design/acceleration.md) | Touching kernels, arch headers, loops, presets, `detect_isa()` or dispatch engines; adding a preset. |
| [doc/performance.md](doc/performance.md) | Measuring or changing speed or code size. Keep it true when a kernel, threshold or loop changes. |
| [doc/crc.md](doc/crc.md), [doc/internet.md](doc/internet.md), [doc/adler32.md](doc/adler32.md), [doc/fletcher.md](doc/fletcher.md), [doc/fletcher4.md](doc/fletcher4.md), [doc/xxhash.md](doc/xxhash.md), [doc/murmur3.md](doc/murmur3.md) | Working on that algorithm (Fletcher and Adler-32 share kernels). User guides: keep them true when behavior, presets or thresholds change. |
