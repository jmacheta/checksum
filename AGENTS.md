# AGENTS.md

A C++23 library of non-cryptographic checksums and hashes, `constexpr` and allocation-free, with CPU kernels selected
by instruction-set presets. Read [README.md](README.md) first for the user-facing model.

## Where things are

| Path | Contents |
| --- | --- |
| `src/include/checksum/` | Public headers: one per algorithm, plus `isa.hpp` (presets), `engine.hpp` (`accumulator`, dispatch engines) and `byte_range.hpp` |
| `src/include_private/checksum_private/` | Portable loops (`<algorithm>_loops.hpp`), kernel templates (`<algorithm>_arch.hpp`), target shape and options (`target.hpp`, `presets.hpp`) |
| `src/include_private/checksum_private/{x86_64,arm,riscv64}/` | Architecture kernels |
| `src/<algorithm>/` | Run-time loops, one source per preset, e.g. `sum_loop_x86_avx2.cpp` |
| `src/isa/detect.cpp` | `detect_isa()`, the only CPU query |
| `src/CMakeLists.txt` | Every library source, listed by hand: add new sources here |
| `tests/unit_tests/` | Google Test, `ut_<algorithm>[_<area>].cpp`, one executable `checksum_ut` |
| `tests/compile_fail/`, `tests/benchmarks/` | Negative compile tests; Google Benchmark, with a stand-in for bare metal |
| `examples/<algorithm>/` | Small complete programs, built but not run |

## Commands

Configure presets are in `CMakePresets.json` and `cmake/*.json`. Build and test presets add `-debug` (and `-release`
for native builds), and binaries go to `build/<preset>/`.

```sh
cmake --workflow --preset native-gcc            # configure, build and test
cmake --preset native-gcc && cmake --build --preset native-gcc-debug
ctest --preset native-gcc-debug -R internet     # the tests of one algorithm
cmake --workflow --preset cross-aarch64         # QEMU; see testing.md for the other cross presets
```

- During development, build and test only what you touched. Run the full matrix of
  [testing.md](doc/design/testing.md#what-to-run) only before finishing a change to shared code.
- Benchmarks and the QEMU matrix are slow: run them rarely, filtered (`--benchmark_filter=`), and only when needed.

## Before finishing

- Run `clang-format -i` on changed C++, `cmake-format -i` on changed CMake and `cspell --config .cspell.yml` on changed
  files, and add no clang-tidy findings (configured in `.clangd`).
- Keep the user guides and [doc/performance.md](doc/performance.md) true when behavior, presets, kernels or thresholds
  change.
- Do not commit unless asked.

## Documents

| File | Read it when |
| --- | --- |
| [doc/design/principles.md](doc/design/principles.md) | Adding or changing an API or algorithm. |
| [doc/design/code_style.md](doc/design/code_style.md) | Writing or reviewing code. |
| [doc/design/build_and_tooling.md](doc/design/build_and_tooling.md) | Touching CMake, presets, dependencies, CI or the lint checks. |
| [doc/design/testing.md](doc/design/testing.md) | Writing tests, or choosing which presets to run. |
| [doc/design/acceleration.md](doc/design/acceleration.md) | Touching kernels, loops, presets, `detect_isa()` or dispatch; adding a preset. |
| [doc/performance.md](doc/performance.md) | Measuring or changing speed or code size. |
| `doc/<algorithm>.md` | Working on that algorithm. Fletcher and Adler-32 share kernels. |
