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
| Code that differs under MSVC (intrinsics, `#if _MSC_VER`, GCC-only constructs) | also `cmake --workflow --preset windows-msvc-wine` (MSVC x64 in Wine; setup in `build_and_tooling.md`); also `cmake --workflow --preset windows-clang-cl-wine` for code that differs by `_MSC_VER`, `__clang__` or `__GNUC__`; MSVC ARM64 is build-only locally (`windows-msvc-arm64-wine`) and runs its tests in CI |
| `x86_vpclmul` or `x86_avxvnni` kernels | a CPU with them, or Intel SDE (`sde64 -adl -- checksum_ut`, below); QEMU has neither |
| RVV kernels | also QEMU `-cpu max,vlen=256` and `vlen=512` (no preset covers them; CI runs both) |

Benchmarks and the QEMU matrix are slow: run them only when the change needs them, filtered to what changed.

Test presets build one configuration (Debug) and run `ctest` with 8 jobs; the negative compile tests share a
resource lock because each runs the build tool on the same tree. Keep a whole run fast: about 2 s of tests natively
and 20 s in QEMU user mode. Debug test builds use `-Og`; `cross-arm` keeps `-O0` so the inline assembly is
built without optimization.

Preset tests run every preset the build has, listed in `isa_detail::built_presets` of `checksum/isa.hpp`, and
skip the presets the CPU cannot run, as `detect_isa()` reports (`tests/include/isa_cpu.hpp`). `ut_dispatch.cpp` checks
every dispatch engine against `isa::generic`, for the detected set and every preset alone. CI also runs `native-gcc`
and `native-clang` on GitHub's AArch64 runner, where the AArch64 presets run on hardware.

`CHECKSUM_REQUIRED_ISA` (preset names as in `CHECKSUM_EXPECTED_ISA` below, each with the earlier rungs of its line)
turns those skips into failures: a preset test fails if the CPU cannot run a required preset, a benchmark executable
exits with an error, and `isa_test_support.RequiredPresetsRun` fails if a required preset is unknown, not built, or not
run. Every CI job sets it (`required` in `ci.yml`) to what its CPU guarantees: `x86_avx2` on GitHub's x86-64 runners
(VPCLMULQDQ and AVX-VNNI vary between their CPUs), `aarch64_pmull` on its Arm runners (Neoverse N2) and in QEMU,
`thumb2_crc` and `riscv_zbc,riscv_v` in QEMU `-cpu max`. The bare-metal check asserts its presets by itself.

The `native-gcc` and `native-clang` jobs run the unit tests again in Intel SDE 10.13 emulating Alder Lake (`-adl`),
with VPCLMULQDQ and AVX-VNNI, requiring `x86_avxvnni`; the kit is pinned by URL and sha256 in `ci.yml` (Intel Simplified
Software License, which allows use and redistribution). The full suite takes under a minute in SDE. Locally:

```sh
CHECKSUM_EXPECTED_ISA=x86_avxvnni CHECKSUM_REQUIRED_ISA=x86_avxvnni sde64 -adl -- build/native-gcc/tests/unit_tests/Debug/checksum_ut
```

The `cross-riscv64` jobs run the RVV, detection and dispatch tests again at `-cpu max,vlen=256` and `vlen=512`.

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

What CI never runs: AArch64 without CRC32 or PMULL (no QEMU model or GitHub runner lacks them), 32-bit Arm Linux
without NEON (bare metal covers the `thumb2` and `thumb2_dsp` builds on Cortex-M and -R), MSVC on ARM64 other than
GitHub's `windows-11-arm` (Neoverse N2, so `aarch64_pmull` only), the x86-64 presets of MSVC and clang-cl on
VPCLMULQDQ or AVX-VNNI (SDE runs only the Linux builds), RVV at vector lengths other than 128, 256 and 512 bits, and
detection on FreeBSD and Apple platforms. Check those on hardware.

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
