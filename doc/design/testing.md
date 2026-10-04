# Testing

## Layout

- Unit tests: Google Test, `tests/unit_tests/ut_<algorithm>_<area>.cpp`. Shared test data and helpers are headers in
  `tests/unit_tests/include/`, maintained by hand.
- Negative compile tests: `tests/compile_fail`. Each is a CTest test that builds an `EXCLUDE_FROM_ALL` target and
  passes only if the build output matches the expected diagnostic; each source also builds as a control target
  without the error.
- Benchmarks: Google Benchmark, `tests/benchmarks`, configured only with `CHECKSUM_BENCHMARKS` (the `native-*-bench`
  presets). The test presets and CI neither build nor run them; in a bench build `ctest` runs one smoke test per
  benchmark executable.
- Write tests alongside the component they exercise.

## What to run

| Change | Presets |
| --- | --- |
| Anything | `cmake --workflow --preset native-gcc` and `native-clang` |
| Bit or byte manipulation (tables, slicing, loads) | also `native-asan` (AddressSanitizer + UBSan) |
| Preconditions, exceptions, RTTI | also `native-gcc-no-exceptions`; the unit tests include one executable built with `NDEBUG` |
| Architecture headers or the table loops | also `x86_64-gcc-pclmul`, `native-gcc-portable` and the cross presets of `cmake/emulator_presets.json` (QEMU user mode) |

Run benchmarks and the full cross matrix only when the change needs them, filtered to what changed; they are slow.

Test presets build one configuration (Debug) and run `ctest` with 8 jobs; the negative compile tests share a
resource lock because each runs the build tool on the same tree. Keep a whole run fast: about 2 s of tests natively
and 20 s in QEMU user mode.

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
