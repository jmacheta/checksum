# Build and tooling

## CMake

- All options are defined and documented in the top-level `CMakeLists.txt` (`CHECKSUM_TESTS`, `CHECKSUM_BENCHMARKS`,
  `CHECKSUM_EXAMPLES`, `CHECKSUM_ACCELERATION`); nothing else reads user settings. Keep comments in CMake files to the minimum.
- Sources and headers are added with `target_sources`; headers through file sets (`PUBLIC FILE_SET HEADERS` for the
  installed API, a `PRIVATE` file set for `include_private`), not `target_include_directories`.
- The build never adds instruction-set flags; acceleration follows the user's compiler flags.
- Third-party code comes through CPM in `cmake/requirements.cmake`: GoogleTest when `CHECKSUM_TESTS` is on, Google
  Benchmark and zlib (the benchmark baseline) when `CHECKSUM_BENCHMARKS` is on.

## Presets

- `CMakePresets.json` holds the test presets and the typical native builds (`native-gcc`, `native-clang`,
  `native-asan`, `native-gcc-no-exceptions`, `native-gcc-portable`, `native-clang-libcxx`).
- Everything else is in dedicated files under `cmake/`, included by `CMakePresets.json`: `x86_64_presets.json`,
  `emulator_presets.json` (cross builds tested in QEMU user mode), `benchmark_presets.json` and the shared hidden
  presets of `common_presets.json`. A preset file includes `common_presets.json` itself if it inherits from it.
- Test presets inherit the hidden `test-common` (output on failure, 8 jobs, stop on failure) and test the Debug
  configuration only. Only the `native-*-bench` presets turn on `CHECKSUM_BENCHMARKS`.
- Cross builds have no toolchain files: system, processor, compilers, QEMU emulator and flags are set in the presets.
  Emulator settings are never CMake options.
- Examples are built, not registered as tests.

## Formatting and linting

Apply as part of every edit:

- C++: `clang-format -i` (`.clang-format`: LLVM base, 150 columns).
- CMake: `cmake-format -i` (`.cmake-format`), except the vendored `cmake/CPM.cmake`.
- Spelling: `cspell --config .cspell.yml "**"`. Add project terms to the `words` list of `.cspell.yml`, not inline
  ignore comments.
- `.clangd` configures `clang-tidy`. Its check list is a deliberate decision: do not re-enable denied checks, and use
  `NOLINT` only for false positives, with the reason.

## CI

GitHub Actions on Ubuntu only (`.github/workflows/ci.yml`): the workflows of the native presets and the x86-64 and
cross presets, which build and run the correctness tests only, not the benchmarks.
Formatting and spelling are checked locally, not in CI.
