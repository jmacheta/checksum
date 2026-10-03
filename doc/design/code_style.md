# Code style

Applies to `src/` and the helper headers of `tests/`; examples follow it where it makes sense.

## Layout

- Include guards (`#ifndef CHECKSUM_CRC_HPP`), never `#pragma once`.
- A header lists its public API first, then implementation details, then definitions. Every function defined in a
  header is declared first and defined at the end of the file, callee before caller; `= default` and `= delete`
  stay inline.

## Naming

- `snake_case` everywhere, types and concepts included; template parameters `CamelCase` (`Register`, `Strategy`),
  macros `UPPER_CASE`. `.clangd` enforces this.
- Whole words: `initial_value`, `final_xor`, `reflect_input`, never abbreviations of catalog field names. Data
  members have plain names without a `_` suffix; if a member function already has the name, pick another word
  (`initial()` and `initial_state`).

## Comments

- At most two short sentences, describing the current code only: no history, no "previously".
- The public API in `src/include/checksum` uses Doxygen (`///`, `///<` after a member); everything else uses plain
  `//`. No `@file` blocks, license text or section separators.
- Code never cites the documents, their sections or requirement IDs; a comment states the fact itself. Only the
  catalog and the test-data headers name their data source.

## Errors and preconditions

- Failures during constant evaluation are `std::optional::value()` of an empty optional; there is no `throw`.
- Preconditions use the standard `assert`, only in header-inline entry points. With `NDEBUG` the argument is masked
  or clamped, so a violation never causes undefined behavior.

## API shape

- One obvious way per task: no alternative notations, duplicate convenience functions or operators that restate
  another API.
- Factories of one type share one name (overloads of `create`). Plain `bool`s over small flag enums.
- Prefer references to whole structs over raw pointers for out-of-line kernels.
