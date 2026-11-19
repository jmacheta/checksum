# Project principles

What every algorithm in this repository has to satisfy.

## Goals

- **Modern C++.** C++23, any standard facility (concepts, ranges, `std::span`, `if consteval`, `<bit>`). No compiler
  extensions in the public interface.
- **Memory-efficient.** No dynamic allocation, no global state. The user chooses the trade-off between table size and
  speed, and unused algorithms, parameter sets and strategies cost nothing in the final binary.
- **Fast.** Portable loops that are competitive on their own, plus CPU instructions chosen by naming an
  instruction-set preset (see [acceleration.md](acceleration.md)). Performance claims are measured, never assumed.
- **Portable and embedded-friendly.** Builds with `-fno-exceptions -fno-rtti`, on bare-metal toolchains, on any
  endianness. Results are identical at compile time and at run time, and on every platform.

## Structure

- One public namespace, `checksum`. Each algorithm prefixes its names (`crc_`) and keeps its implementation in its
  own detail namespace (`checksum::crc_detail`).
- Few headers: an algorithm has its main header and, if it has named parameter sets, a catalog header
  (`checksum/crc.hpp`, `checksum/crc_catalog.hpp`, `checksum/internet.hpp`). What every algorithm needs (the
  `byte_range` concept, chunking of non-contiguous ranges) is in `checksum/byte_range.hpp`, with its details in
  `checksum::detail`. Headers shared only by `src/` go to `src/include_private/checksum_private/`.
- Platform-specific code lives only in the architecture headers, one directory per architecture
  (`checksum_private/x86_64/crc.hpp`, `arm/internet.hpp`, ...); each `<algorithm>_arch.hpp` selects the directory.
- `constexpr` first: parameter validation and table generation work at compile time, so named engines are constants
  in read-only data.
- Bounded template bloat: no functions are instantiated per parameter set. Run-time loops are compiled once in
  `src/`, for a small fixed set of types; data crosses non-template boundaries as `std::span<std::byte const>`.
- Value types with public fields and no invalid states where possible; validation happens once, in a factory that
  returns `std::optional`.
- Not thread-safe by design: objects are unsynchronized values, and a `const` engine may be shared.
- Catalogs and test data are maintained by hand and verified by tests; there are no generators.

## Out of scope

- Cryptographic hashes, message authentication and keyed hashes meant to resist attackers (SHA, SipHash): the library
  is for non-cryptographic checksums and hashes only.
- A C API, and features users can build from the public API (frame verification, residue checks).
- Mandatory run-time CPU detection: it is an optional layer on top of the compile-time presets, and programs that make
  no dispatch engine link none of it.
