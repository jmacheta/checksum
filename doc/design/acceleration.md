# CPU acceleration

How algorithms use CPU instructions and which architectures are worth it, per algorithm. Measured figures are in the
user guides ([CRC](../crc.md#8-performance), [Internet checksum](../internet.md#6-performance),
[Fletcher](../fletcher.md#6-performance), [Adler-32](../adler32.md#6-performance),
[MurmurHash3](../murmur3.md#5-performance), [xxHash](../xxhash.md#5-performance),
[fletcher4](../fletcher4.md#6-performance)).

## Rules

- **Compile time only.** Each algorithm's dispatch header (`crc_arch.hpp`, `internet_arch.hpp`, `fletcher_arch.hpp`, `xxh3_arch.hpp`,
  `fletcher4_arch.hpp`) selects one directory per architecture (`x86_64/`, `arm/`, `riscv64/`, `generic/`) and includes its header from there. That header checks the macros the compiler predefines for the flags of each translation unit
  (`__PCLMUL__`, `__ARM_FEATURE_CRC32`, `__riscv_vector`, ...); without them it uses no kernel, mostly by including the
  generic one. The generic directory also covers `CHECKSUM_ACCELERATION=OFF`. No run-time CPU detection, no target attributes, no static state; the build never
  adds instruction-set flags.
- **One interface per algorithm.** Every architecture header defines the same constants (`*_available`, size
  thresholds) and, where they are true, the same functions; callers use them only in `if constexpr` branches.
  Intrinsics appear nowhere else.
- **Shared algorithm, per-architecture primitives.** The algorithm is written once over a small kernel type (load,
  multiply, fold, reduce); an architecture header supplies only the primitives.
- **Constants computed by `constexpr` code** and stored with the tables, so the data layout is the same in every
  build and named engines stay in read-only data.
- **Bit-identical results.** Every accelerated path is tested against the portable one, including in QEMU for each
  architecture; the x86-64 AVX-VNNI kernel, which QEMU cannot run, is tested locally with `-march=native` on a CPU
  that has it. Constant evaluation always runs the portable code.
- **Measure before adding.** A kernel stays only if it beats the portable loops on real hardware at the sizes where
  it runs; thresholds come from measurements.

## CRC: which architectures are worth accelerating

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 with PCLMULQDQ / VPCLMULQDQ | Yes | Carry-less folding is 1.7× (16 B) to 25× (1 MiB) faster than the portable sliced loop; VPCLMULQDQ doubles the 128-bit kernel from 256 B on. |
| x86-64 SSE4.2 `crc32` | Yes, CRC-32C only | One instruction per 8 bytes; covers short inputs where folding does not pay off. |
| AArch64 / AArch32 CRC extension | Yes, CRC-32 and CRC-32C | 17× the portable sliced loop on a Cortex-A72; one unrolled stream already reaches the instruction throughput. |
| AArch64 PMULL | Yes, for the other parameter sets | `llvm-mca` puts the fold loop at 6-33 cycles per 64 bytes across cores, far below the table loops; unmeasured on hardware. Slower than the CRC instructions on most cores, so never used for CRC-32 / CRC-32C. |
| RISC-V RV64 Zbc | Implemented, unproven | Scalar `clmul` needs four instructions per 16-byte fold; the gain depends on the core. Tested in QEMU only. |
| Arm NEON without PMULL | No | A 64×64 carry-less product from `vmull_p8` takes about 60 instructions; 0.3-0.7× of the table loops. |
| Big-endian AArch64 PMULL | No | NEON lane order of initializers and reinterpretations differs between compilers; the CRC instructions are still used. |
| Cortex-M (ARMv7E-M, ARMv8-M) | Nothing to use | No CRC or carry-less instructions. Choose the table strategy and place it in RAM (about 1.5× flash). MCU CRC peripherals are a user strategy, not library code. |
| Interleaved table streams on in-order cores | No | Braided loops help out-of-order cores only; on a Cortex-M4 they are slower and twice the size. |
| LoongArch, POWER8+, s390x | Not done | They have carry-less multiply or CRC instructions; candidates when someone can test them. |

## CRC kernels

- **Folding** (Intel, "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"): four 128-bit
  accumulators fold 64 bytes per iteration, then single 16-byte blocks; a last partial block is folded together
  with the 16 bytes that end the message; a Barrett reduction gives the register. `fold_blocks_with<Reflected,
  Kernel>` implements it once over `pclmul_kernel`, `pmull_kernel` and `clmul_kernel`.
- **One modulus for every width:** a CRC of width W runs in a 64-bit register modulo
  Q = x^(64−W) · (x^W + polynomial), so one kernel per bit order covers widths 1..64 and every register type. The
  160 bytes of constants live in the slicing-by-8 and braided tables in every build.
- **Wide x86 loop** (VPCLMULQDQ, from 256 bytes): eight 256-bit accumulators over 256-byte steps, then four over
  128 bytes and one 64-byte step. Like Intel ISA-L, it then folds the 8 remaining blocks onto the last one in a
  single step, each block with the constants of its own distance (`by_seven` to `by_one`), instead of a chain of
  folds over one block.
- **CRC instructions** run for CRC-32 and CRC-32C with reflected input in a 32-bit register, whatever the initial
  value and final XOR. On x86-64, CRC-32C inputs from 25 bytes fold with PCLMULQDQ and reduce their last 128 bits
  with two `crc32` instructions; from 256 bytes the wide loop leaves two blocks, and `crc32` takes those 32 bytes
  and the last 0..63 message bytes.
- `crc_lut_braided` runs the `crc_lut_sliced` path wherever a kernel applies.

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Arm: Cortex-A72; MCU: Cortex-M4):

- Folding from 16 bytes: 1.4-1.5× the portable loop at 16 B, no loss at any size.
- Kernel inlined into each of the 8 loops (register type × bit order): one shared copy per bit order was up to 31 %
  (GCC) and 43 % (Clang) slower below 256 B, for 2.7-5.5 KB of code saved.
- The wide x86 loop sits behind one non-inlined call: inlined, it gave every caller a stack frame, about 20 %
  slower at 16-64 B. It is inlined into that callee, one copy per register type and bit order: a second call cost
  8-15 % at 256 B; one shared copy per bit order saved 10 KB but was 9 % slower at 256 B.
- The x86 Barrett reduction stays in vector registers: moving the lanes to general registers and back cost about
  3 cycles per move, and keeping them in vector registers made 16-256 B 10-45 % faster.
- Branches that a 256-byte input does not take are marked unlikely, so that path has no taken jumps: CRC-32C at
  256 B went from 0.84 to 0.93 of ISA-L, measured together with its two-block ending (four `crc32` instructions
  instead of a fold over one block and two). CRC-32 and CRC-64/XZ were unchanged.
- Tried at 256 B without gain: starting with four accumulators as ISA-L does (7 % slower), folding all 16 blocks in
  one parallel step (5-10 % slower, 128 more bytes of constants), 64-byte alignment of the constants (within 2 %).
- Compared with Intel ISA-L's AVX2 kernels (GCC 16 `-O2 -march=native`): 1.01× (CRC-32), 0.95× (CRC-32C) and
  0.96× (CRC-64/XZ) at 256 B, against 0.71-0.78× before the single-step fold.
- Each table source has its own copy of the loops it reuses (internal linkage): equal or up to 20 % faster than
  calling across sources.
- Portable braided loop from 128 bytes, where it overtakes slicing-by-8.
- Arm CRC32: 4 words per iteration, then straight-line 2- and 1-word steps; a loop that runs exactly twice costs a
  mispredicted branch (about 10 ns per call) on the A72.
- Byte-table loop: the split variant for out-of-order cores costs 30-40 % on the Cortex-M4, so targets with 32-bit
  pointers run the plain loop for registers wider than 8 bits.

## Internet checksum

A one's complement sum needs only additions, and the order of the bytes does not matter, so the portable loop is
already fast. It adds native `std::size_t` words in two add-with-carry chains and swaps the bytes of the result once:
on x86-64 that is about 8 200 MB/s at 20 B and 55 500 MB/s at 4 KiB, without any instruction-set flags. A kernel has
to beat that, not a byte loop.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX2 | Yes, from 512 B | 64-byte blocks, 32-bit halves summed into 64-bit lanes: 1.4-1.7× the portable loop at 1500 B-4 KiB, 1.2-1.5× at 1 MiB. The call and the reduction make it slower below about 350 B. |
| x86-64 SSE2 (baseline) | No | The same scheme with 16-byte vectors matched the scalar carry loop and was never faster. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available; the AVX2 kernel is near the L1 bandwidth, so the gain is likely small. |
| Compiler auto-vectorization | No | GCC `-O3 -march=native` vectorizes a 32-bit-word loop to about 47 000 MB/s, below the scalar carry loop; Clang does not vectorize it. |
| AArch64 NEON (`uadalp`) | Yes, from 512 B | Cortex-A72: 1.5× at 1500 B, 1.8× at 4 KiB. The 64-bit portable loop is fast (5 700 MB/s at 256 B), so the kernel loses below about 370 B. Eight accumulators (128 B per iteration) were slower than four. |
| AArch64 `ldp` + `adcs` assembly | No, for now | 1.3-1.5× the portable loop at 256 B-4 KiB on the A72, better than NEON at 256 B but worse from 1500 B. A second kernel for 256-511 B is not worth the assembly. |
| 32-bit Arm NEON | Yes, from 192 B | A72 in AArch32: 3.6× at 4 KiB, where the portable loop runs 32-bit words. |
| 32-bit Arm without NEON (Cortex-M) | Yes, from 192 B, inline assembly | `ldm` + `adcs` chain: 0.69 cycles per byte against 1.27 for the portable loop on a Cortex-M4 (nRF52840 and STM32L4A6): 1.8× at 4 KiB, 1.3× at 256 B, up to 2.5× from an odd address. |
| Cortex-M0/M0+/M23 (Thumb-1) | Not done | `adcs` and `ldm` exist for low registers, but there is no `teq`: the loop test must not clobber the carry. No hardware to measure. |
| Big-endian NEON | No | `vreinterpret` lane order differs between GCC and Clang, as for PMULL; big-endian AArch64 runs the portable loop. |
| RISC-V V extension | Implemented, unproven | Widening add `vwaddu.wv` into 64-bit lanes, vector-length agnostic; tested in QEMU with VLEN 128-1024. No hardware measured, so the 64 B threshold is a guess. |
| RISC-V scalar (Zba, Zbb) | No | No carry flag: a 64-bit word costs a load and three instructions with or without `add.uw`, the same as the portable loop. |

Cortex-M methods, measured on the nRF52840 and STM32L4A6 (cycles per byte at 4 KiB):

| Method | Cycles per byte | Notes |
| --- | --- | --- |
| Portable loop (two 32-bit carry chains) | 1.27 | GCC materializes each carry with `it`/`mov` instead of `adc`. |
| 64-bit accumulators of 32-bit words | 1.46 | `adds` + `adc #0` per word: no better than the portable loop of the same build (1.46). |
| `__builtin_addc` chain | - | GCC 14 still materializes every carry; the generated loop is longer than the portable one. |
| `ldm` of 4 + `adcs` | 0.78 | One `ldm` per 16 bytes. |
| 2 × `ldm` of 4 + `adcs` (library) | 0.70 | 32 bytes per iteration; clobbers only r2-r5, so it builds at `-O0` with a frame pointer. An odd start sums from the next byte and swaps the bytes of the result. |
| `ldm` of 8 + `adcs` | 0.66 | Needs eight registers besides the frame pointer (r7 or r11) and r9; Thumb code with a frame pointer runs out of registers. |
| 2 × `ldm` of 8 + `adcs` | 0.61 | 64 bytes per iteration: 7 % more at 4 KiB, with twice the loop and the register problem of `ldm` of 8. |
| DSP (`uxtah`, `uadd16`, `usada8`) | - | At least two instructions per word, against one `adcs`. |
| MCU CRC unit | - | It computes CRCs only. |

What is left on Cortex-M: inputs below 192 bytes run the portable loop, dominated by the call and the final fold: 122
cycles for 20 bytes, and 346 cycles at 191 B against 273 for the `ldm` kernel at 192 B, so the threshold could move
lower.

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Cortex-M4: nRF52840):

- Two carry chains of 64-bit words: 1.3× one chain at 64 B, 2.6× at 4 KiB. A four-chain prototype was up to 1.3×
  faster again with Clang but 4× slower with GCC, which kept its array in memory; untried with separate variables.
  Each chain takes four words per 64-byte block; on x86-64 `__builtin_addcll` makes it `add` + 3 `adc` + `adc $0`
  like Linux `csum_partial`, which took 256 B from 0.84× to 0.97× of that function. GCC 14 on Arm materializes each carry of the builtin, so Arm and the
  other targets keep the carry count (`adds` + `cinc`). Four chains of two words were not faster on x86-64.
- The bytes after the last whole word come from one load of the message's last word with the summed bytes masked off,
  rotated by one byte when the length is odd; with the rotating fold this made 20 B 1.7× faster on the A72 (0.62× to 1.10× Linux `do_csum`,
  which over-reads instead). Inputs shorter than one word take the 4-, 2- and 1-byte loads before any other branch;
  behind the word steps they cost 10-13 % at 7 B. Skipping the masked load for whole-word lengths keeps 64-256 B fast.
- `fold()` adds a value to its rotation by half its width, two steps instead of four.
- No `memcpy` of variable size for the tail: it called the library function and made a 20-byte input 3.5× slower.
- One final fold after the carry chains are merged: three separate folds cost 8-20 % at 20-64 B.
- The AVX2 and NEON kernels work in passes of 2^26 blocks (4 GiB), the vector kernel in passes of 2^18 iterations, so
  their 64-bit lanes never overflow.
- The final fold works in the width of the native word: on a Cortex-M4 a 64-bit fold cost 30 cycles per call.
- `sum_loop()` holds no call for short inputs: the kernel and the loop after it sit in a separate non-inlined function.
  A call inside `sum_loop()` saved registers on every input and cost 13-23 % below 256 bytes on x86-64; one shared,
  non-inlined copy of the portable loop saved 100 bytes on a Cortex-M4 but cost 25 % at 20 bytes on x86-64.
- Thresholds sit above the point where the kernel overtakes the portable loop of the same build: x86-64 at about 350
  bytes, AArch64 at 370, AArch32 NEON and `ldm` on the A72 at 100-130, the Cortex-M4 `ldm` kernel at 120-160 depending
  on the start address.
- The `ldm` kernel returns its last carry separately: added back into the 32-bit sum, it would be lost when the sum is
  0xFFFFFFFF.

## Fletcher and Adler-32

Fletcher-16, Fletcher-32, Fletcher-64 and Adler-32 share one kernel interface in `fletcher_arch.hpp`: `kernel<Bits>`
sums little-endian values of 8, 16 or 32 bits and returns, for a chunk of n values v_i, their sum and the weighted sum
of (n − i) · v_i. The caller adds n · `sum1` plus the weighted sum to `sum2` and reduces both sums after every chunk.
Fletcher-16 and Adler-32 run the same byte kernel with moduli 255 and 65521. The portable loop, the chunk loop and the
choice between them are written once, in `fletcher_loops.hpp`, for both families.

The portable loops are the baseline. They reduce only when an overflow could otherwise happen (every 380 million bytes
on 64-bit targets for Fletcher-16 and Adler-32) and add four blocks per step, so a byte loop still runs 7 300 MB/s on
x86-64 at 4 KiB with GCC, but only 0.84 GiB/s on a Cortex-A72 and 5.3 cycles per byte on a Cortex-M4.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX-VNNI | Yes, bytes from 64 B | `vpdpbusd` weights 32 bytes in one instruction instead of `pmaddubsw` + `pmaddwd` + add; four blocks per iteration weigh 127 .. 0 against one `previous` addition. Adler-32 at 4 KiB: 84 504 MB/s against 58 088 for the AVX2 kernel (1.45×), 1.13× zlib-ng's AVX-VNNI kernel. Not in a test preset: QEMU has no AVX-VNNI and GitHub runners may lack it, so it is tested locally with `-march=native`. |
| x86-64 AVX2 | Yes: bytes from 64 B, 16-bit values from 128 B, 32-bit values from 384 B | At 4 KiB with GCC, 7.4× the portable loop for Fletcher-16, 7.9× for Adler-32, 3.7× for Fletcher-32, 1.9× for Fletcher-64. |
| x86-64 SSE2 (baseline), SSSE3 | Yes, bytes and 16-bit values | Fletcher-16 3.1× and Fletcher-32 1.5× the portable loop at 4 KiB with GCC. SSSE3 `pmaddubsw` weights 16 bytes in two instructions; plain SSE2 widens the bytes first. |
| x86-64 SSE2, 32-bit values | Not done | The 64-bit portable loop of Fletcher-64 already runs 27 500 MB/s at 4 KiB. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available. |
| AArch64 NEON | Yes: bytes from 64 B, 16-bit from 128 B, 32-bit from 256 B | Cortex-A72 at 4 KiB: about 7× for Fletcher-16 and Adler-32, 2.8× for Fletcher-32, 1.45× for Fletcher-64, whose 64-bit portable loop is already fast. |
| AArch64 NEON, 64 bytes per iteration (`group_kernel`) | Yes, bytes from 320 B | Pairwise additions of four blocks into `sum`, 64 column sums weighted 64 .. 1, one `previous` addition per 64 bytes, as zlib-ng: Adler-32 at 4 KiB 6 270 against 4 746 MB/s (1.32×), 0.96× zlib-ng and ISA-L. Below 320 B weighting 64 columns costs more than it saves. In AArch32 (16 NEON registers) it spilled and lost 10-27 %. |
| 32-bit Arm NEON, little-endian | Yes, the same kernels and thresholds except `group_kernel` | A72 in AArch32 at 4 KiB: 4.7× for Fletcher-16, 5.0× for Adler-32, 2.9× for Fletcher-32, 3.1× for Fletcher-64. |
| M-profile Arm DSP (Cortex-M4/M7/M33), bytes | Yes, from 64 B | `usada8` sums 4 bytes, `uxtb16` + `smlad` weight them: 2.69 cycles per byte against 5.30 on a Cortex-M4 (nRF52840 and STM32L4A6), 2.0× at 4 KiB, 1.6× at 256 B. |
| M-profile Arm DSP, 16-bit and 32-bit values | No | The portable loop sums 32-bit words on 32-bit targets: 2.11 cycles per byte for Fletcher-32 and 1.80 for Fletcher-64, within 2 % of a DSP kernel of the same scheme. |
| A-profile 32-bit Arm without NEON | No | The DSP kernel is slower than the portable loop on the Cortex-A72 in AArch32; only M-profile cores use it. |
| Big-endian NEON | No | The kernels read vector lanes as little-endian values; big-endian targets run the portable loop. |
| RISC-V V extension | Not done | No hardware to measure; a candidate. |

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Arm: Cortex-A72; MCU: Cortex-M4):

- Thresholds are where the kernel overtakes the portable loop of the same build. x86-64 bytes at 64 B with GCC and
  Clang, SSE2 and AVX2, except Fletcher-16 with GCC and SSE2 (7 % slower at 64 B, faster from 96); 16-bit values at
  96 B with Clang and 128-160 with GCC; 32-bit values at 160 B with Clang and 384 with GCC. NEON bytes at 64 B, 16-bit
  values above 64 B and 32-bit values at 256 B, in AArch64 and AArch32.
- Chunk sizes keep the vector lanes from overflowing: 1024 blocks for x86-64 bytes, 128 for 16-bit values, 256
  blocks of 16 bytes for NEON bytes (16-bit column sums), 700 blocks of 8 bytes for the DSP kernel (32-bit weighted
  sum). The 32-bit-value kernels take 65536 values per chunk, so the weighted sum fits 64 bits.
- The chunk sums are reduced in 32-bit arithmetic wherever every intermediate result fits, with the high half of a
  64-bit value folded with the weight 2^32 modulo M, so the modulo is a 32-bit operation on every target.
- The kernel and the portable loop after it sit in a separate non-inlined function, as for the Internet checksum, so
  inputs below the threshold run without a call. `group_kernel` has its own such function, reached by a tail call:
  inside the same function its registers slowed the 16-byte kernel by 10-15 % at 64-256 B.
- The 64-byte NEON kernel takes chunks of 360 blocks, so the weighted sum of a chunk fits 32 bits and the chunk loop
  stays in 32-bit arithmetic; with 1016 blocks it went to 64 bits and was 15 % slower.
- Short inputs, against a plain deferred-modulo loop (the Wikipedia one), at 20 bytes with GCC on x86-64:
  - GCC returned a small aggregate by storing its members one by one and loading them as one word, which stalls store
    forwarding. The sums are built in one integer and copied (`make_state()`), and Fletcher-16 and -32 pass their
    state to `sum_loop()` as one integer (`state_word`): Fletcher-16 from 0.68× to 1.0×, Fletcher-32 from 0.51× to
    0.74×.
  - Unfinished blocks go to a separate non-inlined function, so the whole-block path needs no frame; there the byte
    loops are inline. Fletcher-64, whose state passes in two registers, and 32-bit targets lost 3-10 % that way and
    keep them in `sum_any()`, with out-of-line byte loops.
  - The portable loop ends with straight-line steps of 2 and 1 values on 64-bit targets, and bounds its 4-value loop by
    a pointer; on the Cortex-M4 the pointer cost Fletcher-64 3 % at 4 KiB, so 32-bit targets count.
  - On 64-bit targets the Fletcher moduli reduce 64-bit sums with a constant `%`, a multiplication: the fold loop's
    branches made Fletcher-32 on the A72 swing between 0.55 and 0.85× of the reference with the input size. Adler-32
    keeps the fold, which was 5-10 % faster for 65521, and 32-bit targets would call a division function.
  - AArch64 sums 16-bit values in 32-bit sums: Fletcher-32 at 20 B 0.80 to 0.86× on the A72; on x86-64 it was 9 %
    slower, so x86-64 keeps 64-bit sums.
- The DSP kernel sums single bytes up to a 4-byte aligned address first: word loads from other addresses made the
  Cortex-M4 loop 15-25 % slower. From an odd address it takes 2.71 cycles per byte against 2.69.
- The DSP kernel is not inlined into the chunk loop: there GCC reloaded two weights in every iteration.
- On 32-bit targets the portable Fletcher-32 loop adds 32-bit words, two blocks each, and Fletcher-64 adds its blocks
  one by one: its 64-bit sums make the multiplications of the four-block formula cost more than the dependency chain.
- `CHECKSUM_TEST_ARM_DSP` (preset `cross-arm-portable`) enables the DSP kernel on an A-profile core, only so that QEMU
  user mode can test it.

## MurmurHash3

| Target | Verdict | Why |
| --- | --- | --- |
| Any | No kernel | Each block updates the hash lanes through a rotation, a multiplication and an addition of their previous values, and in x64_128 the second lane takes the first lane of the same block. One serial chain per message leaves nothing for vector instructions. |

The block loop is compiled once per width in `src/murmur3/block_loop.cpp` and takes the lanes by reference, which keeps
the array out of the stack arguments and the return slot. On 32-bit targets x64_128 builds its 64-bit multiplications
from 32-bit ones: on the A72 in AArch32 it runs 552 MB/s at 4 KiB against 1 681 in AArch64, and on a Cortex-M4 it
takes 4.42 cycles per byte against 3.02 for x86_32.

## XXH32 and XXH64

| Target | Verdict | Why |
| --- | --- | --- |
| Any | No kernel | Each of the four lanes is a serial multiply-rotate chain, and the scalar loop already runs the four side by side. XXH32: a NEON prototype ran 1 685 MB/s at 4 KiB on the A72 against 2 333 for the scalar loop, and Clang vectorizing the lanes under LTO lost 38 % on x86-64. XXH64: SSE2, AVX2 and NEON have no 64-bit lane multiplication. |

The stripe loop is compiled once per width in `src/xxhash/stripe_loop.cpp` and is not inlined: behind the call the
lanes may alias the data, which keeps compilers from packing them into one vector register. The four lanes converge
in spelled-out code; as a loop, GCC reloaded the lanes it had just stored as one vector, which stalls.

The one-shot hash keeps the lanes in local variables, in one out-of-line function per width: XXH32 on every target,
XXH64 only on 64-bit targets, since on Cortex-M4 its 64-bit lanes spilled and ran 18 % slower. XXH64 went from 0.88
to 1.0× xxHash v0.8.4 at 64 B on x86-64. An empty `asm` statement on the lanes after the stripe loop keeps GCC from
merging the last multiplication of each lane into the convergence, which kept two values per lane in the loop and cost
7 % at 1-2 KiB.

## XXH3

Inputs up to 240 bytes run straight-line code (0-16, 17-128 and 129-240 bytes, each separately) and never reach a
kernel. Longer inputs fold 64-byte stripes into eight 64-bit accumulators: per lane a 32 × 32 → 64-bit product of the
halves of data XOR secret, plus the neighbor lane's data, and a scramble after each block of 16 stripes.
`fold_stripes<Kernel>` implements the loop once over `portable_kernel` and each architecture's `stripe_kernel`
(load, store, accumulate, scramble).

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 SSE2 (baseline) | Yes, every stripe loop | GCC 16: 21 992 MB/s at 1 MiB against 13 809 for the portable loop (1.6×), 1.5× at 256 B; Clang 21: 32 658 against 20 107. The reference `xxhash.h` 0.8.4 runs 23 179 with SSE2 and 9 175 scalar. |
| x86-64 AVX2 | Yes, every stripe loop | GCC 16: 48 994 MB/s at 1 MiB (3.5× the portable loop), 2.6× at 256 B; Clang 21: 46 907. The reference runs 47 996. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available. |
| AArch64 NEON, all eight lanes | No | Cortex-A72: slower than the portable loop from 256 B on, 2.21 against 2.57 GiB/s at 256 B and 3.64 against 3.93 at 4 KiB. |
| AArch64 NEON, four lanes in NEON and four scalar | Yes, every stripe loop | A72, GCC `-O2`: 2 643 against 2 238 MB/s at 256 B, 4 704 against 3 771 at 4 KiB (1.25×); at `-O3` it ties the portable loop at 256 B and is 2 % faster at 4 KiB. Six NEON lanes and two scalar ones (the reference's split) ran 4 374 MB/s at 4 KiB and 3 807 at 1 MiB against 4 719 and 4 029; all eight in NEON were slower too. |
| 32-bit Arm NEON, little-endian | Yes, every stripe loop | A72 in AArch32, GCC `-O2`: 1 544 against 921 MB/s at 256 B, 2 807 against 1 341 at 4 KiB (2.1×), where the portable loop builds 64-bit additions from 32-bit ones. |
| Big-endian NEON | No | The kernels read vector lanes as little-endian values; big-endian targets run the portable loop. |
| RISC-V V extension, VLEN ≥ 128 | Implemented, unproven | The eight accumulators in one register group (`vuint64m4_t`); about ten vector instructions per stripe instead of about fifty scalar ones. Tested in QEMU only. |
| Cortex-M | No kernel | No vector unit; the portable loop runs. |

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Arm: Cortex-A72):

- Kernel thresholds: x86-64 and AArch32 NEON from the first stripe, since both kernels beat the portable loop at
  256 B, the shortest input that reaches them; so does AArch64, where the four-plus-four kernel is 1.18× the portable
  loop at 256 B with `-O2` and ties it with `-O3`.
- The AArch64 kernel spells out its vector and scalar lanes: as loops, GCC at `-O2` did not unroll them and kept the
  lanes in memory, which made the kernel 25 % slower than the portable loop (2 831 against 3 798 MB/s at 4 KiB).
  Builds at `-O3` unrolled them, which hid the problem.
- Software prefetch 384 bytes ahead, as the reference does, gained 7 % at 1 MiB on the A72 and lost 1-3 % from 1 to
  4 KiB; the kernel does not prefetch.
- AArch32 keeps all eight lanes in NEON: an empty scalar array kept GCC from holding the lanes in registers.
- The RVV kernel requires `__riscv_v_min_vlen >= 128` and 64-bit elements: at VLEN 64 the register group holds only
  four lanes.
- Seed 0 uses the default secret in read-only data; another seed derives the 192-byte secret on the stack in each call
  that folds stripes. In streaming that is each update that folds the buffer: 25 % slower than seed 0 in 256-byte
  pieces, 6 % in 4 KiB pieces.
- The streaming update works in place; `xxh3_update` copies the 336-byte state in and out once per call. In 64-byte
  pieces that gives 2.4 GiB/s against 9.3 for an in-place update with the SSE2 kernel, about 4×; from 4 KiB pieces the
  copy is negligible. The `byte_range` overload folds all its chunks into one state without copying it.
- The accumulate step is spelled out per lane: as a loop, GCC kept the accumulators in memory. The last stripe of the
  message runs in the same loop as the others.

## fletcher4

The word loop adds one word per step through a chain of four dependent additions. Four interleaved lanes break the
chain: lane j sums the words j, j + 4, j + 8, ..., and `combine()` turns the lane sums into the message sums with
fixed integer weights. Word i of n weighs 1, n − i, C(n − i + 1, 2) and C(n − i + 2, 3) in `sum1` to `sum4`; for an
even n the binomial coefficients have closed forms modulo 2^64 (the division by 3 is a product with its inverse), so a
long input pays a fixed cost of a few multiplications. All sums wrap modulo 2^64, so no chunking is needed.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX2 | Yes, from 128 B | One 256-bit vector per sum. 29 258 MB/s at 4 KiB with GCC against 19 318 for the SSE2 kernel (1.5×); 28 620 with Clang. |
| x86-64 SSE2 (baseline) | Yes, from 192 B | Two 128-bit vectors per sum. With GCC the portable lanes are within 15 % of it from 1500 B; with Clang 19 246 MB/s at 4 KiB against 8 424 for the portable lanes. |
| AArch64 NEON, little-endian | Yes, from 192 B | A72 at 256 B: 2 209 MB/s with GCC, as fast as its portable lanes; 2 101 with Clang against 1 500 for its portable lanes. |
| 32-bit Arm NEON, little-endian | Yes, from 192 B | A72 in AArch32 at 4 KiB: 2 384 MB/s with GCC against 1 069 for the word loop (2.2×), 2 862 with Clang. |
| Portable lanes, 64-bit targets | Yes, from 256 B | On the A72 they overtake the word loop at 128 B with GCC; with Clang, whose word loop is unrolled, they stay below it (1 500 MB/s at 256 B against 1 813 for the word loop at 128 B). |
| Portable lanes, 32-bit targets | No | The 64-bit sums of four lanes do not fit the registers: 3× slower than the word loop. Without a kernel the lane code is not even linked. |
| Big-endian NEON | No | The kernel reads vector lanes as little-endian words; big-endian AArch64 runs the portable lanes. |
| RISC-V V extension | Not done | No hardware to measure; RV64 runs the portable lanes. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available. |

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Arm: Cortex-A72):

- Thresholds are where the lanes overtake the word loop of the same build, with GCC and Clang: AVX2 at 128 B; SSE2 at
  192 B (GCC's SSE2 lanes are slower at 128 and 160); NEON at 192 B on AArch64 and AArch32, where Clang's unrolled word
  loop wins below (GCC's NEON lanes win from 80 B on AArch64).
- `combine()` is written out with constant weights, which become shifts and additions. As a loop over a weight table
  GCC vectorized it into emulated 64-bit vector multiplications, which cost more than the lanes saved up to 512 B.
- `compute_loop()` passes its own return value to the long path by reference. A `fletcher4_value` returned by a call is
  an array local, which `-fstack-protector-strong` (on by default in some distributions) guards with a canary check on
  every call, short ones included.
- Words are assembled from four byte loads, which compilers merge into one load where unaligned loads are allowed;
  elsewhere this avoids a call to `memcpy`.
- The lane loop and the word loop after it sit in a separate non-inlined function, as for the Internet checksum, so
  inputs below the threshold run without a call; `fletcher4_compute` has its own copy that starts from the empty state.
- Against OpenZFS f79c81d (`fletcher_4_native` with its superscalar4, sse2, ssse3, avx2 and aarch64_neon kernels, GCC
  `-O2`), the library runs at 0.93× the fastest at 20 B and 1.09× at 256 B on x86-64, 0.92× and 1.18× on the A72, and
  1.0× from 1500 B on x86-64 and from 4 KiB on the A72.
