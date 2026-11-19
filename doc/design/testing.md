# Testing

## Layout

- Unit tests: Google Test, `tests/unit_tests/ut_<algorithm>.cpp`, or `ut_<algorithm>_<area>.cpp` when an algorithm has
  several. Shared test data and helpers are headers in `tests/unit_tests/include/` (and
  `tests/include/` for what benchmarks share), maintained by hand.
- Negative compile tests: `tests/compile_fail`. Each is a CTest test that builds an `EXCLUDE_FROM_ALL` target and
  passes only if the build output matches the expected diagnostic; each source also builds as a control target
  without the error.
- Benchmarks: Google Benchmark, `tests/benchmarks`, configured only with `CHECKSUM_BENCHMARKS` (the `native-*-bench`
  presets). The test presets and CI neither build nor run them; in a bench build `ctest` runs one smoke test per
  benchmark executable. Calls are independent, so short-input figures are throughput, not latency.
- Write tests alongside the component they exercise.

## What to run

During development run only the tests of the algorithm you touched: `ctest --preset native-gcc-debug -R <regex>`
with the suite names (`internet`, `adler32`, `fletcher`, `[Cc]rc`, `xxh`, `murmur3`; MurmurHash3 and XXH32/XXH64
presets are in `hash_presets`, dispatch engines in `dispatch`). Before finishing run the rows below that apply; the
full matrix only for shared code.

| Change | Presets |
| --- | --- |
| Anything | `cmake --workflow --preset native-gcc` and `native-clang` |
| Bit or byte manipulation (tables, slicing, loads) | also `native-asan` (AddressSanitizer + UBSan) |
| Preconditions, exceptions, RTTI | also `native-gcc-no-exceptions`; the unit tests include one executable built with `NDEBUG` |
| x86-64-only code (`x86_64/` headers, x86 preset sources) | the native presets only; presets the CPU lacks are skipped |
| Arm or RISC-V code (`arm/`, `riscv64/` headers, their preset sources) | also the cross presets of that architecture in `cmake/emulator_presets.json` (QEMU user mode, `-cpu max`) |
| Shared code: `*_loops.hpp`, the table loops, `generic/` headers, `isa.hpp`, `detect.cpp` | also every cross preset: `cross-aarch64`, `cross-aarch64be`, `cross-arm`, `cross-arm-crc32`, `cross-riscv64` |
| Code that differs under MSVC (intrinsics, `#if _MSC_VER`, GCC-only constructs) | also `cmake --workflow --preset windows-msvc-wine` (MSVC x64 in Wine; setup in `build_and_tooling.md`) |
| The x86-64 AVX-VNNI Fletcher/Adler-32 kernel | a CPU with AVX-VNNI; QEMU and CI runners may lack it, and the tests skip there |
| RVV vector lengths other than 128 bits | also QEMU `-cpu max,vlen=256` (no preset covers it) |

Benchmarks and the QEMU matrix are slow: run them only when the change needs them, filtered to what changed.

Test presets build one configuration (Debug) and run `ctest` with 8 jobs; the negative compile tests share a
resource lock because each runs the build tool on the same tree. Keep a whole run fast: about 2 s of tests natively
and 20 s in QEMU user mode. Debug test builds use `-Og`; `cross-arm` keeps `-O0` so the inline assembly is
built without optimization.

Preset tests run every preset the build has, from `isa_detail::first_built` to `last_built` of `checksum/isa.hpp`, and
skip the presets the CPU cannot run, as `detect_isa()` reports (`tests/include/isa_cpu.hpp`). `ut_dispatch.cpp` checks
every dispatch engine against `isa::generic`, for the detected set and every preset alone. CI also runs `native-gcc`
and `native-clang` on GitHub's AArch64 runner, where the AArch64 presets run on hardware.

## Coverage expectations

- Every parameter set of a catalog against its published check value, at compile time and at run time.
- Reference vectors computed once by an independent bitwise model, for byte and bit lengths across block boundaries.
- Every strategy and every acceleration path against the portable result: all lengths up to past the largest block,
  unaligned starts, one-shot and split.
- Sweeps follow the code, not full cross products: lengths are exhaustive up to about two steps of the widest loop,
  then sampled; one aligned and one misaligned start where no loop branches on the address; the catalog-wide
  acceleration test runs every length for the first parameter set of each code path, the thresholds ±1 for the rest.
- Edge parameters (smallest and largest width, all-ones values, mixed reflection), rejected parameters, and
  precondition violations (death tests without `NDEBUG`, clamped results with it).
- QEMU checks correctness only; performance is measured on hardware.
