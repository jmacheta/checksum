# CPU acceleration

How presets, kernels, dispatch and detection are structured, and the rules a change must follow.

## Rules

### Presets and sources

- **The preset selects kernels, never a compiler macro.** Code tests it with `isa_detail::includes(Isa, isa::<preset>)`,
  and each `<algorithm>_arch.hpp` includes one architecture directory (`x86_64/`, `arm/`, `riscv64/`).
- **One source per preset and loop.** `src/<algorithm>/<loop>_<preset>.cpp` builds only where `presets.hpp` defines the
  preset's target options, and compiles the loops under `target_begin.hpp`/`target_end.hpp` after every std header. The
  build system adds no flags.
- **RISC-V has no target pragma:** RVV kernels carry a target attribute, and Zbc uses inline assembly.
- **No shared state between preset sources.** Every helper is a template on the preset or has internal linkage, and
  there is no static state.
- **An engine runs its loop preset:** `isa_detail::loop_preset(Isa, own_loops)`, the preset itself or the nearest one it
  extends that is in the algorithm's own-loop set, and `preset()` returns it. So an algorithm needs sources only for the
  presets of its own-loop set.
- **No source that compiles to its parent's code.** An algorithm passes such presets to
  `built_with_target_options({...})`, and they run their parent's loop. Before adding a source, compare its disassembly
  with the parent's (`objdump -d`, preset names stripped) with GCC and Clang, and on Cortex-M for the Thumb-2 presets:
  identical code gets no source.
- **Algorithm-specific presets** (`isa_detail::algorithm_specific()`: `x86_sha`, `aarch64_sha2`, `aarch64_sha512`) are
  left out of `built_with_target_options()`, so only the algorithms that list them build sources for them.
- **Generic-only loops** are built once, for `generic`, and their own-loop set is `isa_set{}`: `crc_lut_none`,
  `crc_lut_nibble`, `crc_lut_byte`, XXH32, XXH64, MurmurHash3 and SHA. Add per-preset copies only if they beat it on
  hardware.

### Kernels

- **One kernel shape.** A kernel is a class template on the preset whose primary template means "no kernel"
  (`available = false`, `minimum_size` at its maximum). Architecture headers specialize it with a `requires` clause on
  the preset, `available = true`, the shortest input it runs for, and static functions.
- **Intrinsics appear only in kernels,** which loops use in `if constexpr(<kernel>::available)` branches.
- **Target differences go through `target.hpp`:** `arm64_target` (every compiler, MSVC included), `arm32_target`,
  `word32_target`, `in_order_target<Isa>` and `neon_target<Isa>`.
- **Intrinsic types per preset.** Arm and RISC-V sections that need NEON, DSP or RVV types compile only where
  `CHECKSUM_PRESET_HAS(...)` is 1 for the source's preset.
- **Algorithm once, primitives per architecture:** an architecture header supplies only loads, multiplies and folds.
- **Constants come from `constexpr` code** and are stored with the tables, so named engines stay in read-only data.
- **No call below the threshold.** A kernel and the loop after it sit in a non-inlined function, so short inputs run the
  inline portable loop.
- **Vector kernels are little-endian only,** because GCC and Clang order NEON lanes differently on big-endian targets.

### Correctness and thresholds

- **Results are bit-identical.** Every kernel is tested against the portable loop in QEMU.
- **A kernel must beat the portable loop on hardware.** Its threshold is where it overtakes the portable loop of the
  same build. Kernels tested only in QEMU are marked "unproven".

### Dispatch and detection

- **`detect_isa()`** in `src/isa/detect.cpp` is the only CPU query. It is not `constexpr`, because constant evaluation
  would silently make `static` engines generic.
- **Dispatch engines** pick a built preset with `isa_detail::select()` and call it via `isa_detail::dispatch()`. They
  choose only from the algorithm's own-loop set (`<algorithm>_detail::own_loops`): `generic`, the presets with target
  options and code of their own, and the RISC-V presets the algorithm has a kernel for. A `static_assert` in each loops header checks the set.
- **Detection never guesses.** A preset is reported if `compiler_guarantees()` finds it in the predefined macros, or if
  the CPU or OS reports it: CPUID on x86-64, `getauxval` and `riscv_hwprobe` on Linux, `IsProcessorFeaturePresent` on
  Windows, `elf_aux_info` on FreeBSD and `sysctlbyname` on Apple.

### Compiler constraints

- **Clang on 32-bit Arm** inlines no ARM-state function into Thumb code, and `target("neon")` with `-mfpu=vfpv3-d16`
  crashes LLVM 20 and 21. So its `thumb2` presets need `-mthumb`, and `thumb2_neon` and `thumb2_crc` the build's own
  NEON.
- **MSVC** has one NEON type, `__n128`, so do not overload on NEON types. Out-of-class definitions of constrained
  specializations need a trailing return type when it is a member type.
- **clang-cl** reads CPUID through `<intrin.h>`, so it needs no compiler-rt.
- **No SIMD vectors in `std::array`:** clang-cl under-aligns them inside MS STL templates. Use
  `isa_detail::vector_array`.

## Adding a preset

1. Append the enumerator to `enum class isa` in `checksum/isa.hpp`, never reordering, and add it to
   `isa_detail::line()` if it extends a line.
2. Add it to `isa_detail::built_presets` for its architecture and compiler, after the earlier rungs.
3. Add `CHECKSUM_TARGET_<PRESET>` options to `presets.hpp` under the same conditions.
4. Copy a sibling source for each loop with kernels (adler32, crc `lut_sliced` and `lut_braided`, fletcher, fletcher4,
   internet, xxh3) and list the new sources in `src/CMakeLists.txt`. Where a copy compiles to its parent's code, delete it
   and add the preset to the algorithm's `built_with_target_options({...})` list instead. A preset for a few algorithms only goes into
   `algorithm_specific()` instead, and only those algorithms add it to their own-loop sets and get sources.
5. Specialize the kernels with `requires(isa_detail::includes(Isa, isa::<preset>))`. Add
   `CHECKSUM_PRESET_HAS_<FEATURE>_<preset>` if they need intrinsic types the build may lack.
6. Add the CPU check to `detect.cpp` (`checked` and `runs()`) and the macros to `compiler_guarantees()`.
7. Add the name to `tests/include/isa_cpu.hpp`.
8. If it compiles without target options, as the RISC-V presets do, add it to `without_target_options()` and to the
   own-loop set of each algorithm that has a kernel for it.
9. Update the presets tables of the user guides and the ladder in the README.

## CRC

Folding follows Intel's PCLMULQDQ paper, written once in `fold_blocks_with<Reflected, Kernel>` over the carry-less
multiply primitive of each architecture. A CRC of width W runs in a 64-bit register modulo x^(64−W) · (x^W + P), so one
kernel per bit order serves every width and register. Its 160 bytes of constants live in the sliced and braided tables.

| Preset | Kernel |
| --- | --- |
| `x86_pclmul` and up | PCLMULQDQ folding from 16 B; CRC-32C ends with two SSE4.2 `crc32` instead of Barrett |
| `x86_vpclmul` and up | Adds a 256-bit VPCLMULQDQ loop from 256 B |
| `x86_avx512` | Its 256-byte steps in four 512-bit vectors (unproven) |
| `aarch64_crc`, `thumb2_crc` | CRC32/CRC32C instructions for CRC-32 and CRC-32C |
| `aarch64_pmull` | PMULL folding for other CRCs, little-endian (unproven) |
| `riscv_zbc` | Zbc folding, little-endian (unproven) |
| others | Portable sliced or braided loop |

- The CRC instructions serve any CRC with the CRC-32 or CRC-32C polynomial and reflected input in a 32-bit register.
- `crc_lut_braided` runs the sliced path wherever a kernel applies, and its portable loop pays only on out-of-order
  cores, so 32-bit targets run the byte-table loop.

## Internet checksum

The portable loop sums native words in two add-with-carry chains and byte-swaps once at the end; kernels must beat it.

| Preset | Kernel |
| --- | --- |
| `x86_avx2` and up | 32-bit halves in 64-bit lanes, from 512 B; 512-bit vectors first on `x86_avx512` (unproven) |
| little-endian AArch64, `generic` included | NEON `uadalp`, from 512 B |
| `thumb2_neon`, `thumb2_crc`, little-endian | NEON, from 192 B |
| `thumb2`, `thumb2_dsp`, and NEON presets on big-endian | `ldm` + `adcs` assembly, from 192 B |
| `riscv_v` | Widening `vwaddu`, from 64 B (unproven) |
| others | Portable loop |

- No load may reach past the message: the tail is one masked load of the last word.
- Only x86-64 has a `carry_chain`, since `__builtin_addc` materializes every carry on Arm.
- The `ldm` kernel returns its last carry separately and clobbers only r2-r5, so it builds at `-O0`.

## Fletcher and Adler-32

All four checksums share `block_kernel<Bits>` in `fletcher_arch.hpp`, which returns the sum and the weighted sum of a
chunk of little-endian values. Fletcher-16 and Adler-32 run the same byte kernel with moduli 255 and 65521.

| Preset | Kernel (8 / 16 / 32-bit values) |
| --- | --- |
| `x86_avx512` | 64-byte vectors with 512-bit `vpdpbusd`, from 64 / 128 / 384 B (unproven) |
| `x86_avxvnni` | `vpdpbusd` from 64 B / as AVX2 / as AVX2 |
| `x86_avx2`, `x86_vpclmul` | 32-byte vectors from 64 / 128 / 384 B |
| `generic`, `x86_pclmul` on x86-64 | 16-byte vectors from 64 / 128 B / none |
| little-endian AArch64 | NEON from 64 / 128 / 256 B; bytes use `group_kernel` from 320 B |
| `thumb2_neon`, `thumb2_crc`, little-endian | NEON, without `group_kernel` |
| `thumb2_dsp`, little-endian | `usada8` + `smlad`, bytes only, from 64 B |
| `riscv_v` | Strips of one register group, any vector length, from 64 / 128 / 256 B (unproven) |
| others | Portable loop |

- Chunk sizes keep vector lanes from overflowing, in every kernel of this file and the Internet checksum.
- Short inputs build the sums in one integer, because member-wise stores stall store forwarding with GCC.
- Adler-32 reduces with its fold (2^16 ≡ 15 mod 65521), not a division, so 32-bit targets call no division function.

## MurmurHash3, XXH32, XXH64 and FNV-1a

These have no kernels: each lane is a serial chain of multiplications. Per-preset copies of their loops gave no
speed-up and cost code size, so each has one generic loop.

- On 32-bit Arm, MurmurHash3 x86_32 takes four blocks per iteration so that multiplications overlap.
- The XXH32/XXH64 stripe loop is not inlined, so compilers do not pack the lanes into one vector.
- XXH64's one-shot keeps its lanes in locals only on 64-bit targets, since they spill on Cortex-M.
- FNV-1a has no loop in `src/`: its inline loop takes four bytes per iteration, 1.55× faster on Cortex-M4.

## SHA-1 and SHA-2

SHA's own-loop set holds `x86_sha`, `aarch64_sha2` and `aarch64_sha512` only (`isa_detail::only()`), so every other
preset runs generic's loop. A kernel replaces the strategy's rounds; `finalize()` and the buffered block go through the
same loop, so short messages gain too.

| Preset | Kernel |
| --- | --- |
| `x86_sha` | SHA-NI for SHA-1 and SHA-256, four rounds per group, fully unrolled with the schedule interleaved |
| `aarch64_sha2`, little-endian | SHA1 and SHA256 instructions (unproven) |
| `aarch64_sha512`, little-endian, not MSVC | Adds the SHA512 instructions, two rounds per step (unproven) |

## XXH3

Inputs up to 240 bytes run straight-line code and never reach a kernel. Longer inputs fold 64-byte stripes into eight
accumulators through `stripe_kernel`, whose copy is first declared inside the target region.

| Preset | Kernel |
| --- | --- |
| `generic`, `x86_pclmul` on x86-64 | Four 128-bit vectors |
| `x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | Two 256-bit vectors |
| `x86_avx512` | One 512-bit vector (unproven) |
| little-endian AArch64 | Four lanes in NEON, four scalar |
| `thumb2_neon`, `thumb2_crc`, little-endian | Eight lanes in NEON |
| `riscv_v` (VLEN ≥ 128) | One `vuint64m4_t` group (unproven) |
| others | Portable loop |

- 32-bit targets call the out-of-line `run_time_multiply_fold()`, so the 17-128 byte path fits small instruction caches.

## fletcher4

Four interleaved lanes break the word loop's dependency chain, and `combine()` merges them with closed-form weights
modulo 2^64.

| Preset | Lanes |
| --- | --- |
| `x86_avx2` and up | One 256-bit vector per sum, from 128 B (four lanes: AVX-512 would gain nothing) |
| `generic`, `x86_pclmul` on x86-64 | Two 128-bit vectors per sum, from 192 B |
| little-endian AArch64; `thumb2_neon`, `thumb2_crc` | NEON, from 192 B |
| `riscv_v` | Four 64-bit lanes per register group, from 192 B (unproven) |
| other 64-bit presets | Portable lanes, from 256 B |
| other 32-bit presets | Word loop only, two words per iteration on Arm |

- `compute_loop()` returns its result by reference, which avoids a stack-protector check per call.
