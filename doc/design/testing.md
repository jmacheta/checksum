# Testing

## Layout

- Unit tests: Google Test, one executable `checksum_ut` (and `checksum_ut_unchecked`, built with `NDEBUG`). Every
  algorithm has the same files, suites and cases; see [Unit test layout](#unit-test-layout). Shared test data and
  helpers are headers in `tests/unit_tests/include/` (and `tests/include/` for what benchmarks share), maintained by
  hand.
- Negative compile tests: `tests/compile_fail`. Each is a CTest test that builds an `EXCLUDE_FROM_ALL` target and
  passes only if the build output matches the expected diagnostic; each source also builds as a control target
  without the error. `accumulator_from_temporary`, `string_literal_update` and `unavailable_preset` build once per
  algorithm (`compile_fail.<algorithm>_<source>`; `test_engine.hpp` picks the engine from `CHECKSUM_TEST_<ALGORITHM>`);
  the rejected-parameter and precondition tests exist for CRC only, the one algorithm with parameters and asserts.
- Benchmarks: Google Benchmark, `tests/benchmarks`, configured only with `CHECKSUM_BENCHMARKS` (the `native-*-bench`
  and `bare-cortex-*` presets). In a bench build `ctest` runs one smoke test per benchmark executable. Before it
  measures, each benchmark of a preset or a dispatch engine checks its results against `isa::generic` and exits with an
  error on a mismatch. Calls are independent, so short-input figures are throughput, not latency.
- Bare metal: the `bare-cortex-*` presets (`cmake/bare_metal_presets.json`, Arm GNU Toolchain `arm-none-eabi` on
  PATH) build the benchmarks and no unit tests, with `bare_metal_benchmark.cpp` standing in for Google Benchmark and
  `startup.S`; their smoke tests run in QEMU system mode with semihosting. The stand-in's `static_assert`s check the
  presets that the flags promise against `CHECKSUM_EXPECTED_ISA` and `CHECKSUM_EXPECTED_BUILT`, and at start-up it
  checks `detect_isa()` and dispatch selection. `startup.S` enables the FPU (and on Cortex-A a flat MMU map, without
  which unaligned accesses fault) and ends QEMU through semihosting, also on a fault.
- Write tests alongside the component they exercise.

## Unit test layout

Algorithms are `adler32`, `crc`, `fletcher`, `fletcher4`, `internet`, `murmur3`, `xxh3` and `xxhash`. Suites and cases are
`snake_case`; a suite is the algorithm or `<algorithm>_<area>`, death-test suites end in `_DeathTest`. An algorithm
with several widths is a typed suite with one instance per width (`fletcher/fletcher16`, `murmur3/x86_32`,
`xxhash/xxh64`, `xxh3/xxh3_128`); a preset suite has one instance per built preset (`internet_presets/x86_avx2`).

| File | Suite | Contents |
| --- | --- | --- |
| `ut_<algorithm>.cpp` | `<algorithm>` | The canonical cases below and the algorithm's own cases |
| `ut_<algorithm>_presets.cpp` | `<algorithm>_presets` | Every built preset against `isa::generic` |
| `ut_accumulator.cpp` | `accumulator` | A static and a dispatch engine of every algorithm and width; CRC bit-length updates |
| `ut_dispatch.cpp` | `dispatch`, `dispatch_presets` | Every dispatch engine and width against `isa::generic`, for the detected set and each preset alone |
| `ut_isa.cpp` | `detect_isa`, `isa_test_support` | Presets, `detect_isa()` and the preset helpers of the tests |
| `ut_crc_<area>.cpp` | `crc_<area>` | CRC only: `catalog`, `parameters`, `polynomial`, `strategies`, `vectors`, `unchecked` (`NDEBUG`) |

Cases of `ut_<algorithm>.cpp`, the same name for the same check:

| Case | Checks |
| --- | --- |
| `reference_values` | Published values, reference vectors or an independent model |
| `every_size_and_offset` | Every size past the largest block, from several start offsets, against the model or the vectors |
| `split_anywhere` | Every split into three updates of a short message (`engine_checks::check_split_anywhere`) |
| `split_into_chunks` | Pieces of every size up to past the blocks; a short prefix before long pieces |
| `long_message` | About 1 MiB (20 KB for CRC), one-shot and in 4099-byte pieces |
| `byte_ranges` | Strings, `unsigned char` vectors and non-contiguous ranges equal the span overload |
| `constant_evaluation` | Prefixes computed at compile time equal the run-time results |
| `edge_values` | Extreme states, seeds and parameters: largest sums, block offsets, all-ones seeds, every CRC width |
| `resume_from_checksum` | Resumable engines only: a stored result continues the message |
| `equal_states_after_any_split` | Hashes with a buffered state (`xxhash`, `xxh3`, `murmur3`): states of one message split differently compare equal |
| `long_all_ones` | Sum checksums with deferred reduction (`adler32`, `fletcher`, `fletcher4`): the largest bytes from the largest sums over many reductions |
| `chunk_boundaries` | `adler32`, `fletcher`: around every kernel's chunk sizes, from every word alignment, all-ones and random bytes |

Cases of `ut_<algorithm>_presets.cpp`, with the checks of `engine_checks.hpp`:

| Case | Checks |
| --- | --- |
| `constant_evaluation` | The preset's engine and `<algorithm>_compute<..., preset>` during constant evaluation and at run time |
| `match_generic_for_every_size` | Every size up to 2048 bytes (MurmurHash3 and XXH32/64: 1024, CRC: 640) from every start offset modulo 8 |
| `match_generic_for_large_sizes` | Sizes around 512 and 1024 bytes and up to about 1 MB, four start offsets |
| `match_generic_through_updates_and_splits` | Three updates split around blocks, buffers and thresholds; resume, empty updates, ranges |
| `match_generic_for_all_ones` | Sum checksums only (Internet, Adler-32, Fletcher, fletcher4): all-ones runs around every kernel's chunk sizes from the largest sums |

CRC runs these on CRC-32C (`crc_lut_sliced`) and CRC-64/XZ (`crc_lut_braided`), plus
`match_byte_table_for_common_parameters`; `crc_folding_presets` checks every catalog set on `crc_lut_sliced` and
`crc_lut_braided` of every preset, `isa::generic` included. An algorithm's own cases, such as `field_update` or
`lanes_continue_state`, keep descriptive names.

## What to run

During development run only the tests of the algorithm you touched: `ctest --preset native-gcc-debug -R <regex>`
with the algorithm name (`adler32`, `crc`, `fletcher`, `internet`, `murmur3`, `xxh3`, `xxhash`), which matches its
unit, preset and compile-fail tests; `fletcher` also matches `fletcher4`, and dispatch engines are in `dispatch`. Before
finishing run the rows below that apply; the full matrix only for shared code.

| Change | Presets |
| --- | --- |
| Anything | `cmake --workflow --preset native-gcc` and `native-clang` |
| Bit or byte manipulation (tables, slicing, loads) | also `native-asan` (AddressSanitizer + UBSan) |
| Preconditions, exceptions, RTTI | also `native-gcc-no-exceptions`; the unit tests include one executable built with `NDEBUG` |
| x86-64-only code (`x86_64/` headers, x86 preset sources) | the native presets only; presets the CPU lacks are skipped |
| Arm or RISC-V code (`arm/`, `riscv64/` headers, their preset sources) | also the cross presets of that architecture in `cmake/emulator_presets.json` (QEMU user mode, `-cpu max`) |
| Shared code: `*_loops.hpp`, `*_arch.hpp`, the table loops, `target.hpp`, `presets.hpp`, `isa.hpp`, `detect.cpp` | also every cross preset: `cross-aarch64`, `cross-aarch64be`, `cross-arm`, `cross-arm-crc32`, `cross-arm-clang`, `cross-arm-crc32-clang`, `cross-riscv64`, `cross-riscv64-clang` |
| `detect.cpp`, `compiled_isa.hpp`, the 32-bit Arm presets | also the `bare-cortex-*` workflows (`qemu-system-arm`) and the CPU checks below |
| Code that differs under MSVC (intrinsics, `#if _MSC_VER`, GCC-only constructs) | also `cmake --workflow --preset windows-msvc-wine` (MSVC x64 in Wine; setup in `build_and_tooling.md`); also `cmake --workflow --preset windows-clang-cl-wine` for code that differs by `_MSC_VER`, `__clang__` or `__GNUC__`; MSVC ARM64 is build-only locally (`windows-msvc-arm64-wine`) and runs its tests in CI |
| `x86_vpclmul` or `x86_avxvnni` kernels | a CPU with them, or Intel SDE (`sde64 -adl -- checksum_ut`, below); QEMU has neither |
| RVV kernels | also QEMU `-cpu max,vlen=256` and `vlen=512` (no preset covers them; CI runs both) |

Benchmarks and the QEMU matrix are slow: run them only when the change needs them, filtered to what changed.

Test presets build one configuration (Debug) and run `ctest` with 8 jobs; the negative compile tests share a resource
lock because each runs the build tool on the same tree. Keep a whole run fast: about 2 s of tests natively and 20 s in
QEMU user mode. Debug test builds use `-Og`; `cross-arm` and `cross-arm-clang` keep `-O0` so the inline assembly is
built without optimization.

Preset tests run every preset the build has, listed in `isa_detail::built_presets` of `checksum/isa.hpp`, and
skip the presets the CPU cannot run, as `detect_isa()` reports (`tests/include/isa_cpu.hpp`). `ut_dispatch.cpp` checks
every dispatch engine against `isa::generic`, for the detected set and every preset alone. CI also runs `native-gcc`
and `native-clang` on GitHub's AArch64 runner, where the AArch64 presets run on hardware.

`CHECKSUM_REQUIRED_ISA` (preset names as in `CHECKSUM_EXPECTED_ISA` below, each with the earlier rungs of its line)
turns those skips into failures: a preset test fails if the CPU cannot run a required preset, a benchmark executable
exits with an error, and `isa_test_support.required_presets_run` fails if a required preset is unknown, not built, or not
run. Every CI job sets it (`required` in `ci.yml`) to what its CPU guarantees: `x86_avx2` on GitHub's x86-64 runners
(VPCLMULQDQ and AVX-VNNI vary between their CPUs), `aarch64_pmull` on its Arm runners (Neoverse N2) and in QEMU,
`thumb2_crc` and `riscv_zbc,riscv_v` in QEMU `-cpu max` (`riscv_zbc` alone with Clang 18, which builds no `riscv_v`
in an rv64gc build). The bare-metal benchmarks assert their presets by themselves.
QEMU reports Zbc in `riscv_hwprobe` from 9.0 on, so the RISC-V jobs take QEMU 10.2 (Ubuntu 26.04's statically linked
`qemu-user`, pinned by sha256 in `ci.yml`) instead of Ubuntu 24.04's 8.2, where `detect_isa()` misses `riscv_zbc`.

The `native-gcc` and `native-clang` jobs run the unit tests again in Intel SDE 10.13 emulating Alder Lake (`-adl`),
with VPCLMULQDQ and AVX-VNNI, requiring `x86_avxvnni`; the kit is pinned by URL and sha256 in `ci.yml` (Intel Simplified
Software License, which allows use and redistribution). The full suite takes under a minute in SDE. Locally:

```sh
CHECKSUM_EXPECTED_ISA=x86_avxvnni CHECKSUM_REQUIRED_ISA=x86_avxvnni sde64 -adl -- build/native-gcc/tests/unit_tests/Debug/checksum_ut
```

The `cross-riscv64` and `cross-riscv64-clang` (Clang 19) jobs run the RVV, detection and dispatch tests again at `-cpu max,vlen=256` and `vlen=512`.

`detect_isa.matches_the_cpu` (`ut_isa.cpp`, Linux) requires exactly the built presets that the CPU runs: those that
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

What CI never runs: AArch64 without CRC32 or PMULL (no QEMU model or GitHub runner lacks them), 32-bit Arm Linux without
NEON (bare metal runs the `thumb2` and `thumb2_dsp` builds on Cortex-M3/M4/M7/M33 and Cortex-A9 in QEMU;
`bare-cortex-m0`, `bare-cortex-r5` and `bare-cortex-r52` have no QEMU machine and are only built, with their
`static_assert` checks), MSVC on ARM64 other than GitHub's `windows-11-arm` (Neoverse N2, so `aarch64_pmull` only), the
x86-64 presets of MSVC and clang-cl on VPCLMULQDQ or AVX-VNNI (SDE runs only the Linux builds), RVV at vector lengths
other than 128, 256 and 512 bits, and detection on FreeBSD and Apple platforms. Check those on hardware.

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
