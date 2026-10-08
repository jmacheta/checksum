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
| [doc/crc.md](doc/crc.md) | Working on the CRC: user-visible behavior, strategies, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/internet.md](doc/internet.md) | Working on the Internet checksum: API, behavior, acceleration, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/fletcher.md](doc/fletcher.md) | Working on the Fletcher checksums or the shared Fletcher/Adler-32 kernels: API, behavior, acceleration, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/adler32.md](doc/adler32.md) | Working on Adler-32 or the shared Fletcher/Adler-32 kernels: API, behavior, acceleration, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/murmur3.md](doc/murmur3.md) | Working on MurmurHash3: API, behavior, why it has no kernel, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/xxhash.md](doc/xxhash.md) | Working on XXH32, XXH64 or XXH3: API, streaming cost, acceleration, why XXH32 and XXH64 have no kernel, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/fletcher4.md](doc/fletcher4.md) | Working on ZFS fletcher4: definition and relation to ZFS, API, lane kernels and thresholds, performance guidelines, limitations. Keep it true when behavior changes. |
| [doc/performance.md](doc/performance.md) | Measuring or changing the speed or code size of any algorithm: the measured figures per platform. Keep it true when a kernel, threshold or loop changes. |
