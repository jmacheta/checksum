# Testing

## Layout

- Unit tests: Google Test, `tests/unit_tests/ut_<algorithm>.cpp`, or `ut_<algorithm>_<area>.cpp` when an algorithm has
  several. Shared test data and helpers are headers in `tests/unit_tests/include/` (and
  `tests/include/` for what benchmarks share), maintained by hand.
- Negative compile tests: `tests/compile_fail`. Each is a CTest test that builds an `EXCLUDE_FROM_ALL` target and
  passes only if the build output matches the expected diagnostic; each source also builds as a control target
  without the error.
- Bare metal: `tests/bare_metal/isa_check.cpp`, one program without Google Test, built instead of the unit tests when
  `CMAKE_SYSTEM_NAME` is `Generic` (`cmake/bare_metal_presets.json`, Arm GNU Toolchain `arm-none-eabi` on PATH). Its
  `static_assert`s check the presets that the flags promise and build against the preset's `CHECKSUM_EXPECTED_ISA` and
  `CHECKSUM_EXPECTED_BUILT`; at run time it checks `detect_isa()` and every dispatch engine against `isa::generic`.
  `startup.S` enables the FPU (and on Cortex-A a flat MMU map, without which unaligned accesses fault) and ends QEMU
  through semihosting, also on a fault.
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
| `detect.cpp`, `compiled_isa.hpp`, the 32-bit Arm presets | also the `bare-cortex-*` workflows (`qemu-system-arm`) and the CPU checks below |
| Code that differs under MSVC (intrinsics, `#if _MSC_VER`, GCC-only constructs) | also `cmake --workflow --preset windows-msvc-wine` (MSVC x64 in Wine; setup in `build_and_tooling.md`) |
| The x86-64 AVX-VNNI Fletcher/Adler-32 kernel | a CPU with AVX-VNNI; QEMU and CI runners may lack it, and the tests skip there |
| RVV vector lengths other than 128 bits | also QEMU `-cpu max,vlen=256` (no preset covers it) |

Benchmarks and the QEMU matrix are slow: run them only when the change needs them, filtered to what changed.

Test presets build one configuration (Debug) and run `ctest` with 8 jobs; the negative compile tests share a
resource lock because each runs the build tool on the same tree. Keep a whole run fast: about 2 s of tests natively
and 20 s in QEMU user mode. Debug test builds use `-Og`; `cross-arm` keeps `-O0` so the inline assembly is
built without optimization.

Preset tests run every preset the build has, listed in `isa_detail::built_presets` of `checksum/isa.hpp`, and
skip the presets the CPU cannot run, as `detect_isa()` reports (`tests/include/isa_cpu.hpp`). `ut_dispatch.cpp` checks
every dispatch engine against `isa::generic`, for the detected set and every preset alone. CI also runs `native-gcc`
and `native-clang` on GitHub's AArch64 runner, where the AArch64 presets run on hardware.

`detect_isa.MatchesTheCpu` (`ut_isa.cpp`, Linux) requires exactly the built presets that the CPU runs: those that
`CHECKSUM_EXPECTED_ISA` names (comma-separated preset names, each with the earlier rungs of its line), or else the
feature list of `/proc/cpuinfo`, where only `riscv_zbc` may be missing (older kernels and QEMU have no Zbc key). It
skips without either; QEMU user mode emulates `/proc/cpuinfo` for Arm and RISC-V only in newer versions, so the
Arm test presets set the variable for `-cpu max`. CI also runs the detection and dispatch tests on CPUs without
extensions (`cpu_checks` in `ci.yml`); locally, e.g.:

```sh
CHECKSUM_EXPECTED_ISA=generic qemu-x86_64 -cpu Nehalem build/native-gcc/tests/unit_tests/Debug/checksum_ut \
  --gtest_filter='detect_isa.*:dispatch*'   # also Westmere: x86_pclmul, Haswell: x86_avx2
CHECKSUM_EXPECTED_ISA=thumb2_neon qemu-arm -cpu cortex-a9 build/cross-arm/tests/unit_tests/Debug/checksum_ut ...
CHECKSUM_EXPECTED_ISA=generic qemu-riscv64 -cpu max,zbc=false,v=false build/cross-riscv64/tests/unit_tests/Debug/checksum_ut ...
```

No QEMU AArch64 model lacks CRC32 or PMULL, and TCG has no VPCLMULQDQ or AVX-VNNI: AArch64 CPUs without those and
x86-64 CPUs with these are checked on hardware only.

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
