# Testing

## Layout

- Unit tests: Google Test, `tests/unit_tests/ut_<algorithm>_<area>.cpp`. Shared test data and helpers are headers in
  `tests/unit_tests/include/`, maintained by hand.
- Negative compile tests: `tests/compile_fail`. Each is a CTest test that builds an `EXCLUDE_FROM_ALL` target and
  passes only if the build output matches the expected diagnostic; each source also builds as a control target
  without the error.
- Benchmarks: Google Benchmark, `tests/benchmarks`, built with the tests. `ctest` runs only a smoke test; measure
  with the `native-*-bench` presets.
- Write tests alongside the component they exercise.

## What to run

| Change | Presets |
| --- | --- |
| Anything | `cmake --workflow --preset native-gcc` and `native-clang` |
| Bit or byte manipulation (tables, slicing, loads) | also `native-asan` (AddressSanitizer + UBSan) |
| Preconditions, exceptions, RTTI | also `native-gcc-no-exceptions`; the unit tests include one executable built with `NDEBUG` |
| Architecture headers or the table loops | also `x86_64-gcc-pclmul`, `native-gcc-portable` and the cross presets of `cmake/emulator_presets.json` (QEMU user mode) |

Run benchmarks and the full cross matrix only when the change needs them, filtered to what changed; they are slow.

## Coverage expectations

- Every parameter set of a catalog against its published check value, at compile time and at run time.
- Reference vectors computed once by an independent bitwise model, for byte and bit lengths across block boundaries.
- Every strategy and every acceleration path against the portable result: all lengths up to past the largest block,
  unaligned starts, one-shot and split.
- Edge parameters (smallest and largest width, all-ones values, mixed reflection), rejected parameters, and
  precondition violations (death tests without `NDEBUG`, clamped results with it).
- QEMU checks correctness only; performance is measured on hardware.
