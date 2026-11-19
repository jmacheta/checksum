# Build and tooling

## CMake

- All options are defined and documented in the top-level `CMakeLists.txt` (`CHECKSUM_TESTS`, `CHECKSUM_BENCHMARKS`,
  `CHECKSUM_EXAMPLES`); nothing else reads user settings. Keep comments in CMake files to the minimum.
- Library sources are listed by hand, no globs, in `target_sources(checksum PRIVATE ...)` of `src/CMakeLists.txt`:
  every per-preset loop source and `isa/detect.cpp`. Unit tests are listed the same way in
  `tests/unit_tests/CMakeLists.txt`. The top-level `CMakeLists.txt` holds only options, dependencies and
  subdirectories.
- Headers are added through file sets (`PUBLIC FILE_SET HEADERS` for the installed API, a `PRIVATE` file set for
  `include_private`), not `target_include_directories`.
- The build never adds instruction-set flags: each preset source carries its own target options (`presets.hpp`,
  `target_begin.hpp`), so any build system compiles every `src/**/*.cpp` as is.
- Third-party code comes through CPM in `cmake/requirements.cmake`: GoogleTest when `CHECKSUM_TESTS` is on, Google
  Benchmark and zlib (the benchmark baseline) when `CHECKSUM_BENCHMARKS` is on.

## Presets

- Configure presets are named after the build (`native-gcc`); their build and test presets add `-debug` (and
  `-release` for the plain native builds), and the workflow preset of the same name runs configure, build and test.
  Binaries go to `build/<preset>/`.
- `CMakePresets.json` holds the typical native builds (`native-gcc`, `native-clang`, `native-asan`,
  `native-gcc-no-exceptions`, `native-clang-libcxx`, `windows-msvc`).
- Everything else is in dedicated files under `cmake/`, included by `CMakePresets.json`: `emulator_presets.json`
  (cross builds tested in QEMU user mode), `benchmark_presets.json`, `msvc_wine_presets.json` and the shared hidden
  presets of `common_presets.json`. A preset file includes `common_presets.json` itself if it inherits from it.
- Test presets inherit the hidden `test-common` (output on failure, 8 jobs, stop on failure) and test the Debug
  configuration only.
- Only the `native-*-bench` presets turn on `CHECKSUM_BENCHMARKS`. They have configure and build presets only (Release,
  `-O2 -march=native`, which affects only the portable loops); run the executables in `build/<preset>/` by hand.
- Cross builds have no toolchain files: system, processor, compilers, QEMU emulator and flags are set in the presets.
  Emulator settings are never CMake options.
- Examples are built, not registered as tests.
- `msvc_wine_presets.json` has `windows-msvc-wine`: real MSVC x64 run through Wine, tests too. Set it up once, outside
  the repo, with [msvc-wine](https://github.com/mstorsjo/msvc-wine): `vsdownload.py --accept-license --dest
  ~/.local/opt/msvc` then `install.sh ~/.local/opt/msvc` (needs `msitools`). It uses the static CRT (GoogleTest's
  default; Wine has no debug CRT DLLs) and `/Z7` (no `mspdbsrv`).

## Formatting and linting

Apply as part of every edit:

- C++: `clang-format -i` (`.clang-format`: LLVM base, 150 columns).
- CMake: `cmake-format -i` (`.cmake-format`), except the vendored `cmake/CPM.cmake`.
- Spelling: `cspell --config .cspell.yml` on the changed files, `"**"` for the whole tree. Add project terms to the
  `words` list of `.cspell.yml`, not inline ignore comments.
- `.clangd` configures `clang-tidy`. Its check list is a deliberate decision: do not re-enable denied checks, and use
  `NOLINT` only for false positives, with the reason.

## CI

GitHub Actions (`.github/workflows/ci.yml`): the workflows of the native presets and the cross presets on Ubuntu, and
`windows-msvc` on Windows; they build and run the correctness tests only, not the benchmarks. Most jobs use the minimum
supported compilers (GCC 14, Clang 18); the jobs named "latest" use the newest GCC (xPack), Clang (apt.llvm.org) and the
Arm and Bootlin cross toolchains of [setup-gcc-toolchain](https://github.com/jmacheta/setup-gcc-toolchain), so a new
compiler release can fail them without a change here.
Formatting and spelling are checked locally, not in CI.
