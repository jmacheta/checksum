# AGENTS.md

Instructions for AI coding agents working in this repository. Read [README.md](README.md) first, then the files
below when the task touches their area.

| File | Read it when |
| --- | --- |
| [doc/design/principles.md](doc/design/principles.md) | Adding or changing any API or algorithm: goals, namespaces and headers, constexpr-first, template bloat, what is out of scope. |
| [doc/design/code_style.md](doc/design/code_style.md) | Writing or reviewing any code: layout, naming, comments, errors and preconditions. |
| [doc/design/build_and_tooling.md](doc/design/build_and_tooling.md) | Touching CMake, presets, dependencies or CI; before finishing an edit (formatting, spelling, clang-tidy). |
| [doc/design/testing.md](doc/design/testing.md) | Writing tests, or deciding which presets to run for a change. |
| [doc/design/acceleration.md](doc/design/acceleration.md) | Touching architecture headers or table loops, or considering a new CPU target. |
| [doc/crc.md](doc/crc.md) | Working on the CRC: user-visible behavior, strategies, measured performance, limitations. Keep it true when behavior changes. |
