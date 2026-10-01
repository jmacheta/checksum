# AGENTS.md

Instructions for AI coding agents working in this repository. Read
[README.md](README.md) first for purpose and build instructions.

## Project facts

- Language: C++23. Namespace `checksum`, with CRC under `checksum::crc`.
- `doc/specification/crc.md` is the authoritative CRC specification (being
  written) - check it before implementing or reviewing CRC behavior.
- Library is **not thread-safe by design** - no internal locking is planned;
  callers must synchronize externally if sharing an engine instance.
- Prefer `constexpr`/`consteval` first: table generation and parameter
  validation should be usable at compile time wherever possible.
- Avoid template bloat: do not template on things that don't need to vary
  (e.g. don't templatize over integer width speculatively); prefer a small
  number of concrete instantiations over a generic hierarchy.

## Formatting and linting

Apply formatting as part of every edit, not as a separate pass:

- After touching any `.c`/`.h`/`.cpp`/`.hpp` file, run `clang-format -i` on
  it immediately. Config: `.clang-format` (LLVM base, 150 column limit).
- After touching any `CMakeLists.txt` or `.cmake` file (except the vendored
  `cmake/CPM.cmake`), run `cmake-format -i` on it immediately. Config:
  `.cmake-format`.
- Spell check with `cspell --config .cspell.yml "**"`. Add project-specific
  terms to `.cspell.yml`'s `words` list rather than inline ignore comments.
- `.clangd` configures `clang-tidy` for the editor/language server. The
  enabled/disabled check list is a deliberate project decision, not an
  oversight - do not re-enable denied checks, and do not silence a real
  finding with `NOLINT` unless it's a false positive for this codebase.

## Testing

- Unit tests use Google Test, in `tests/unit_tests` (`ut_crc_*.cpp`).
- Build and run via presets, e.g.:

      cmake --workflow --preset native-gcc

- For anything doing manual bit/byte manipulation (CRC tables, slicing),
  also run the `native-asan` preset (AddressSanitizer + UndefinedBehavior
  Sanitizer) - out-of-bounds access and signed-overflow/UB can pass under a
  plain build and only surface there.
- Write new or extended tests alongside the component they exercise; there
  is no separate test-writing workflow.
