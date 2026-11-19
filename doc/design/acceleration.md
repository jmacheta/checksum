# CPU acceleration

How the algorithms use CPU instructions: the rules, how to add a preset, and per algorithm which kernel runs for which
preset and why. The user-facing model is in [README.md](../../README.md#instruction-set-presets); measured figures are
in [performance.md](../performance.md) only.

## Rules

- **Compile-time presets.** Every function takes an `isa` preset (`checksum/isa.hpp`) as a template parameter,
  `isa::generic` by default. Each `<algorithm>_arch.hpp` selects one directory per architecture (`x86_64/`, `arm/`,
  `riscv64/`, `generic/`) and includes its header. That header selects kernels with
  `if constexpr(isa_detail::includes(Isa, isa::<preset>))`, never on compiler macros, and uses the portable code
  without a kernel. Every helper is a template on the preset, so each preset's copy has its own name. No static state.
- **One source per preset.** `src/<algorithm>/<loop>_<preset>.cpp` builds only where `checksum_private/presets.hpp`
  defines the preset's target options, and compiles the loops under a target pragma (`target_begin.hpp`,
  `target_end.hpp`) after every std header. The build system adds no flags, and flags such as `-march=native` select no
  kernel. GCC on RISC-V has no target pragma, so the RVV kernels carry their own target attribute.
- **Run-time choice on top.** `detect_isa()` (`src/isa/detect.cpp`, the only source with CPU queries) returns an
  `isa_set`, the default argument of every dispatch engine. An engine turns the set into one built preset with
  `isa_detail::select()` and calls the templates through `isa_detail::dispatch()`. `detect_isa()` is deliberately not
  `constexpr`: a constant-evaluable detection would silently make `static` engines generic.
- **One interface per algorithm.** Every architecture header defines the same constants (`*_available`, size
  thresholds) and, where they are true, the same functions; callers use them only in `if constexpr` branches.
  Intrinsics appear nowhere else.
- **Shared algorithm, per-architecture primitives.** The algorithm is written once over a small kernel type (load,
  multiply, fold, reduce); an architecture header supplies only the primitives.
- **Constants computed by `constexpr` code** and stored with the tables, so the data layout is the same in every build
  and named engines stay in read-only data.
- **Bit-identical results.** Every kernel is tested against the portable loop, in QEMU for each architecture; constant
  evaluation always runs the portable code.
- **Measure before adding.** A kernel stays only if it beats the portable loop on real hardware at the sizes where it
  runs, and its threshold is where it overtakes the portable loop of the same build. Kernels tested only in QEMU are
  marked "unproven" below.
- **Little-endian vector kernels only.** NEON lane order of initializers and reinterpretations differs between GCC and
  Clang on big-endian targets, so big-endian Arm runs the portable code (CRC keeps the CRC32 instructions).
- **No call below the threshold.** Where a kernel exists, the kernel and the loop after it sit in a separate
  non-inlined function, so short inputs run the inline portable loop without a call or a stack frame.

## Adding a preset

1. Append the enumerator to `enum class isa` in `checksum/isa.hpp`; never reorder. If it extends a line, add it to
   `isa_detail::line()`.
2. Add it to `isa_detail::built_presets` for its architecture and compiler, after the earlier rungs.
3. Add `CHECKSUM_TARGET_<PRESET>` target options in `src/include_private/checksum_private/presets.hpp`, under the same
   conditions as that list.
4. Add one source per loop, `src/<algorithm>/<loop>_<preset>.cpp` (copy a sibling: it defines `CHECKSUM_PRESET` and
   `CHECKSUM_PRESET_TARGET` and includes the loops header), for adler32, crc (`lut_sliced`, `lut_braided`), fletcher,
   fletcher4, internet, murmur3, xxh3 and xxhash, and list them in `src/CMakeLists.txt`.
5. Select its kernels in the architecture headers with `if constexpr(isa_detail::includes(Isa, isa::<preset>))`.
6. Add the CPU check in `src/isa/detect.cpp`: the `checked` array and `runs()`. A `static_assert` fails for a built
   preset without one.
7. Add its name to `tests/include/isa_cpu.hpp`; `ut_isa.cpp` fails without it. Preset tests and benchmarks pick it up
   from `built_presets`.
8. Add a preference to `isa_detail::select()` only if the preset opens a second line on one architecture (as
   `riscv_zbc` and `riscv_v`).
9. Update the preset tables of the user guides and the ladder in the README.

## CRC

Folding (Intel, "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"): four 128-bit accumulators
fold 64 bytes per iteration, then 16-byte blocks; a last partial block folds with the 16 bytes that end the message; a
Barrett reduction gives the register. `fold_blocks_with<Reflected, Kernel>` implements it once over `pclmul_kernel`,
`pmull_kernel` and `clmul_kernel`. A CRC of width W runs in a 64-bit register modulo Q = x^(64−W) · (x^W +
polynomial), so one kernel per bit order covers widths 1..64 and every register type; its 160 bytes of constants live
in the sliced and braided tables in every build.

| Preset | Kernel | Why |
| --- | --- | --- |
| `x86_pclmul` and up | PCLMULQDQ folding, every parameter set, from 16 B | Faster than the sliced loop at every size. |
| `x86_pclmul` and up, CRC-32C | Folding from 25 B, last 128 bits reduced with two SSE4.2 `crc32` | Cheaper than a Barrett reduction. |
| `x86_vpclmul` and up | 256-bit VPCLMULQDQ loop from 256 B | Doubles the 128-bit kernel. |
| `aarch64_crc`, `thumb2_crc` | CRC32/CRC32C instructions, CRC-32 and CRC-32C | One unrolled stream reaches the instruction throughput. |
| `aarch64_pmull` | PMULL folding for the other parameter sets, little-endian only | Never for CRC-32/CRC-32C: slower than the CRC instructions. Unproven on hardware. |
| `riscv_zbc` | Zbc `clmul`/`clmulh` folding, little-endian; inline assembly enables Zbc locally | Unproven: tested in QEMU only. |
| every other preset | Portable sliced or braided loop | No CRC or carry-less instructions (Cortex-M, NEON without PMULL, SSE2). |

Not done: SSE4.2-only (no such preset), NEON without PMULL (a `vmull_p8` product is slower than tables), MSVC ARM64,
LoongArch, POWER8+, s390x (candidates when someone can test them). MCU CRC peripherals are a user strategy, not
library code.

Design notes:

- The CRC instructions run for any parameter set with the CRC-32 or CRC-32C polynomial and reflected input in a 32-bit
  register, whatever the initial value and final XOR.
- The 128-bit kernel is inlined into each of the 8 loops (register type × bit order). The wide x86 loop sits behind
  one non-inlined call (inlined, it gave every caller a stack frame) and folds its last 8 blocks in one step with
  per-distance constants (`by_seven` to `by_one`), as ISA-L does; from 256 B CRC-32C leaves two blocks plus the tail
  to `crc32`. Branches a 256-byte input does not take are marked unlikely.
- The x86 Barrett reduction stays in vector registers.
- `crc_lut_braided` runs the `crc_lut_sliced` path wherever a kernel applies. The portable braided loop runs five
  streams of 8-byte words from 128 B, and pays only on out-of-order cores.
- Each table source has its own internal-linkage copy of the loops it reuses.
- Arm CRC32 loops: 4 words per iteration, then straight-line 2- and 1-word steps.
- Targets with 32-bit pointers run the plain byte-table loop; the split variant for out-of-order cores is slower on
  in-order cores.
- Rejected: aligned tables, `std::mdspan` tables (constant-evaluation cost), little-endian loads with `std::byteswap`
  in the sliced loop (kept in the braided loop).

## Internet checksum

The portable loop is the bar: native `std::size_t` words in two add-with-carry chains (on x86-64 `add` + 3 `adc` +
`adc $0`, like Linux `csum_partial`), byte-swapped once at the end, since the order of additions does not change a
one's complement sum.

| Preset | Kernel | Why |
| --- | --- | --- |
| `x86_avx2` and up | 32-bit halves summed into 64-bit lanes, 64-byte blocks, from 512 B | Loses to the scalar carry loop below about 350 B. |
| every little-endian AArch64 preset, `generic` included | NEON `uadalp`, from 512 B | The 64-bit portable loop is fast; NEON overtakes it at about 370 B. |
| `thumb2_neon`, `thumb2_crc`, little-endian | The same NEON kernel, from 192 B | The 32-bit portable loop is slow. |
| `thumb2`, `thumb2_dsp` (and NEON presets on big-endian) | `ldm` + `adcs` inline assembly, 32 bytes per iteration, any start address, from 192 B | GCC materializes each carry of the portable loop. |
| `riscv_v` | Widening `vwaddu` into 64-bit lanes, VLEN-agnostic, from 64 B | Unproven; the threshold is a guess. |
| every other preset | Portable loop | |

Not done or rejected: SSE2 vectors (never beat the carry loop), compiler auto-vectorization, AArch64 `ldp` + `adcs`
assembly (better only at 256-511 B), Thumb-1 (no `teq`; the loop test would clobber the carry), scalar RISC-V (no carry
flag), MSVC ARM64, AVX-512 (no hardware).

Design notes:

- The tail comes from one load of the message's last word with the summed bytes masked off, rotated by one byte for
  odd lengths; no load reaches past the message, and no variable-size `memcpy` (it called the library function).
  Inputs shorter than a word take 4-, 2- and 1-byte loads before any other branch.
- `fold()` adds a value to its rotation by half its width; one final fold after the chains merge, in the native word
  width.
- On Arm `__builtin_addc` materializes every carry, so non-x86 targets count carries with `adds` + `cinc`.
- Vector kernels work in passes small enough that 64-bit lanes never overflow.
- The `ldm` kernel returns its last carry separately: added into a 32-bit sum of 0xFFFFFFFF it would be lost. It
  clobbers only r2-r5, so it builds at `-O0` with a frame pointer.

## Fletcher and Adler-32

Fletcher-16, -32, -64 and Adler-32 share one kernel interface in `fletcher_arch.hpp`: `kernel<Bits>` sums
little-endian values of 8, 16 or 32 bits and returns, for a chunk of n values v_i, their sum and the weighted sum of
(n − i) · v_i; the caller adds n · `sum1` plus the weighted sum to `sum2` and reduces after every chunk. Fletcher-16
and Adler-32 run the same byte kernel with moduli 255 and 65521. The portable loop, chunk loop and the choice between
them are written once, in `fletcher_loops.hpp`.

| Preset | Kernel (bytes / 16-bit / 32-bit values) | Why |
| --- | --- | --- |
| `x86_avxvnni` | `vpdpbusd`, 128 bytes per iteration, from 64 B / as AVX2 / as AVX2 | One instruction weights 32 bytes. Not run in QEMU or CI. |
| `x86_avx2`, `x86_vpclmul` | 32-byte vectors from 64 B / 128 B / 384 B | |
| `generic`, `x86_pclmul` on x86-64 | 16-byte vectors from 64 B / 128 B / none | SSSE3 `pmaddubsw` from `x86_pclmul`; the 64-bit portable Fletcher-64 loop is fast. |
| every little-endian AArch64 preset | NEON from 64 B / 128 B / 256 B; bytes 64 per iteration from 320 B (`group_kernel`) | `group_kernel` as zlib-ng; below 320 B weighting 64 columns costs more than it saves. |
| `thumb2_neon`, `thumb2_crc`, little-endian | NEON, as AArch64 without `group_kernel` | `group_kernel` spills with 16 NEON registers. |
| `thumb2_dsp`, little-endian | `usada8` + `uxtb16` + `smlad`, bytes only, from 64 B | For Cortex-M4/M7/M33. Slower than portable on A-profile; there only for QEMU coverage. 16/32-bit values: the portable 32-bit word loop is as fast. |
| every other preset | Portable loop | |

Not done: RISC-V (no hardware), MSVC ARM64, AVX-512.

Design notes:

- Chunk sizes keep vector lanes from overflowing; chunk sums reduce in 32-bit arithmetic, the high half of a 64-bit
  value folded with the weight 2^32 mod M. The 64-byte NEON kernel takes 360-block chunks so its loop stays 32-bit.
- `group_kernel` has its own non-inlined function, reached by a tail call.
- Short inputs: the sums are built in one integer (`make_state()`, `state_word`, `sum_pair` via `std::bit_cast`) so
  GCC does not store members one by one and stall store forwarding. Below the kernel threshold 64-bit targets run one
  pass without reduction runs (`sum_portable()`). Unfinished blocks go to a separate non-inlined function, except for
  Fletcher-64 and 32-bit targets (`sum_any()`).
- 64-bit targets reduce Fletcher sums with a constant `%`; Adler-32 keeps its fold (2^16 ≡ 15 mod 65521), and 32-bit
  targets would call a division function.
- AArch64 sums Fletcher-32 blocks in 32-bit sums; x86-64 keeps 64-bit sums. On 32-bit targets Fletcher-32 adds 32-bit
  words, two blocks each, and Fletcher-64 adds blocks one by one.
- The DSP kernel sums single bytes up to a 4-byte aligned address first, and is not inlined into the chunk loop.

## MurmurHash3

No kernel on any target: each block updates the lanes through a rotation, a multiplication and an addition of their
previous values, and in x64_128 the second lane takes the first lane of the same block, one serial chain.

- The block loop is compiled once per width and preset in `src/murmur3/` and takes the lanes by reference.
- On 32-bit Arm with any preset but `thumb2_neon` and `thumb2_crc` (Cortex-M, small in-order A-profile cores), x86_32
  takes four blocks per iteration, so the multiplications of later blocks run while the lane chain completes.
  Out-of-order targets keep one block; two x64_128 blocks spill on Cortex-M.

## XXH32 and XXH64

No kernel on any target: each of the four lanes is a serial multiply-rotate chain and the scalar loop already runs
the four side by side. A NEON XXH32 was slower than scalar; SSE2, AVX2 and NEON have no 64-bit lane multiply for XXH64.

- The stripe loop is compiled once per width and preset in `src/xxhash/` and not inlined, so the lanes may alias the
  data and compilers do not pack them into one vector. The lanes converge in spelled-out code.
- The one-shot hash keeps the lanes in local variables in one out-of-line function per width and preset: XXH32
  everywhere, XXH64 only on 64-bit targets (its lanes spill on Cortex-M). An empty `asm` on the lanes after the loop
  keeps GCC from merging the last multiplication into the convergence.

## XXH3

Inputs up to 240 bytes run straight-line code (0-16, 17-128, 129-240) and never reach a kernel. Longer inputs fold
64-byte stripes into eight 64-bit accumulators; `fold_stripes<Kernel>` implements the loop once over `portable_kernel`
and each architecture's `stripe_kernel` (load, store, accumulate, scramble).

| Preset | Kernel | Why |
| --- | --- | --- |
| `generic`, `x86_pclmul` on x86-64 | Four 128-bit vectors, every stripe | |
| `x86_avx2` and up | Two 256-bit vectors, every stripe | |
| every little-endian AArch64 preset | Four lanes in NEON, four scalar | All eight in NEON was slower than portable; four and four beat the reference's six and two. |
| `thumb2_neon`, `thumb2_crc`, little-endian | All eight lanes in NEON | The portable loop builds 64-bit additions from 32-bit ones. |
| `riscv_v` (VLEN ≥ 128) | Eight lanes in one register group (`vuint64m4_t`) | Unproven. At VLEN 64 the group holds only four lanes. |
| every other preset | Portable loop | |

Not done: MSVC ARM64, AVX-512.

Design notes:

- Kernels run from the first stripe: every kernel beats the portable loop at 256 B, the shortest input that reaches it.
- Lanes and accumulate steps are spelled out, not loops: GCC at `-O2` kept looped lanes in memory.
- No software prefetch: it helped only at 1 MiB.
- Seed 0 uses the default secret in read-only data; another seed derives the 192-byte secret on the stack in each call
  that folds stripes.
- Streaming folds in place, but `xxh3_update` copies the 336-byte state in and out per call; the `byte_range` overload
  folds all its chunks into one state.

## fletcher4

The word loop adds one word per step through four dependent additions. Four interleaved lanes break the chain: lane j
sums words j, j + 4, ..., and `combine()` turns lane sums into message sums with fixed weights: word i of n weighs 1,
n − i, C(n − i + 1, 2) and C(n − i + 2, 3). For even n the binomials have closed forms modulo 2^64 (division by 3 is a
product with its inverse), so a long input pays a fixed cost of a few multiplications. All sums wrap modulo 2^64; no
chunking.

| Preset | Lanes | Why |
| --- | --- | --- |
| `x86_avx2` and up | One 256-bit vector per sum, from 128 B | |
| `generic`, `x86_pclmul` on x86-64 | Two 128-bit vectors per sum, from 192 B | |
| every little-endian AArch64 preset | NEON, from 192 B | Clang's unrolled word loop wins below. |
| `thumb2_neon`, `thumb2_crc`, little-endian | NEON, from 192 B | |
| other 64-bit presets (`riscv_zbc`, `riscv_v`, big-endian AArch64) | Portable lanes, from 256 B | |
| other 32-bit presets (`generic`, `thumb2`, `thumb2_dsp`, big-endian Arm) | Word loop only, two words per iteration on 32-bit Arm without NEON | Four 64-bit lane sums do not fit the registers; two words halve the loop overhead on in-order cores. |

Not done: RISC-V V (no hardware), AVX-512.

Design notes:

- `combine()` is written out with constant weights (shifts and additions); as a loop GCC vectorized it badly.
- `compute_loop()` passes its result by reference: a returned `fletcher4_value` array gets a stack-protector canary
  check on every call.
- Words are assembled from four byte loads, which compilers merge where unaligned loads are allowed.
- `fletcher4_compute` has its own copy of the long path that starts from the empty state.
