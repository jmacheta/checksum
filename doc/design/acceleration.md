# CPU acceleration

How algorithms use CPU instructions and which architectures are worth it, per algorithm. Measured figures are in the
user guides ([CRC](../crc.md#8-performance), [Internet checksum](../internet.md#5-performance),
[Fletcher](../fletcher.md#6-performance), [Adler-32](../adler32.md#6-performance),
[MurmurHash3](../murmur3.md#5-performance)).

## Rules

- **Compile time only.** `checksum_private/arch.hpp` selects one directory per architecture (`x86_64/`, `arm/`,
  `riscv64/`, `generic/`), and each algorithm's dispatch header (`crc_arch.hpp`, `internet_arch.hpp`, `fletcher_arch.hpp`) includes its
  header from there. That header checks the macros the compiler predefines for the flags of each translation unit
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
  architecture. Constant evaluation always runs the portable code.
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
  80 bytes of constants live in the slicing-by-8 and braided tables in every build.
- **CRC instructions** run for CRC-32 and CRC-32C with reflected input in a 32-bit register, whatever the initial
  value and final XOR. On x86-64, CRC-32C inputs from 25 bytes fold with PCLMULQDQ and reduce their last 128 bits
  with two `crc32` instructions.
- `crc_lut_braided` runs the `crc_lut_sliced` path wherever a kernel applies.

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Arm: Cortex-A72; MCU: Cortex-M4):

- Folding from 16 bytes: 1.4-1.5× the portable loop at 16 B, no loss at any size.
- Kernel inlined into each of the 8 loops (register type × bit order): one shared copy per bit order was up to 31 %
  (GCC) and 43 % (Clang) slower below 256 B, for 2.7-5.5 KB of code saved.
- The wide x86 loop sits behind one non-inlined call: inlined, it gave every caller a stack frame, about 20 %
  slower at 16-64 B.
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
on x86-64 that is about 8 000 MB/s at 20 B and 49 000 MB/s at 4 KiB, without any instruction-set flags. A kernel has
to beat that, not a byte loop.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX2 | Yes, from 512 B | 64-byte blocks, 32-bit halves summed into 64-bit lanes: 1.6-1.8× the portable loop at 1500 B-4 KiB. The call and the reduction make it slower below about 450 B. |
| x86-64 SSE2 (baseline) | No | The same scheme with 16-byte vectors matched the scalar carry loop and was never faster. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available; the AVX2 kernel is near the L1 bandwidth, so the gain is likely small. |
| Compiler auto-vectorization | No | GCC `-O3 -march=native` vectorizes a 32-bit-word loop to about 47 000 MB/s, below the scalar carry loop; Clang does not vectorize it. |
| AArch64 NEON (`uadalp`) | Yes, from 512 B | Cortex-A72: 1.5× at 1500 B, 1.8× at 4 KiB. The 64-bit portable loop is fast (5 400 MB/s at 256 B), so the kernel loses below about 450 B. Eight accumulators (128 B per iteration) were slower than four. |
| AArch64 `ldp` + `adcs` assembly | No, for now | 1.3-1.5× the portable loop at 256 B-4 KiB on the A72, better than NEON at 256 B but worse from 1500 B. A second kernel for 256-511 B is not worth the assembly. |
| 32-bit Arm NEON | Yes, from 192 B | A72 in AArch32: 4.1× at 4 KiB, where the portable loop runs 32-bit words. |
| 32-bit Arm without NEON (Cortex-M) | Yes, from 192 B, inline assembly | `ldm` + `adcs` chain: 0.70 cycles per byte against 1.27 on a Cortex-M4 (nRF52840 and STM32L4A6): 1.8× at 4 KiB, 1.2× at 256 B, up to 2.5× from an odd address. |
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

What is left on Cortex-M: inputs below 192 bytes run the portable loop, dominated by the call and the final fold (about
100 cycles for 20 bytes).

Decisions, with the measurement behind each (x86-64: Core Ultra 7 155H; Cortex-M4: nRF52840):

- Two carry chains of 64-bit words: 1.3× one chain at 64 B, 2.6× at 4 KiB. A four-chain prototype was up to 1.3×
  faster again with Clang but 4× slower with GCC, which kept its array in memory; untried with separate variables.
- The tail is loaded in fixed 4-, 2- and 1-byte steps: a `memcpy` of variable size called the library function and
  made a 20-byte input 3.5× slower.
- One final fold after the carry chains are merged: three separate folds cost 8-20 % at 20-64 B.
- The AVX2 and NEON kernels work in passes of 2^26 blocks (4 GiB), the vector kernel in passes of 2^18 iterations, so
  their 64-bit lanes never overflow.
- The final fold works in the width of the native word: on a Cortex-M4 a 64-bit fold cost 30 cycles of the 150 that
  a 20-byte input took.
- `sum_loop()` holds no call for short inputs: the kernel and the loop after it sit in a separate non-inlined function.
  A call inside `sum_loop()` saved registers on every input and cost 13-23 % below 256 bytes on x86-64; one shared,
  non-inlined copy of the portable loop saved 100 bytes on a Cortex-M4 but cost 25 % at 20 bytes on x86-64.
- Thresholds are where the kernel overtakes the portable loop of the same build: x86-64 at about 450 bytes, AArch64 at
  450, AArch32 NEON at 150-190, the Cortex-M4 `ldm` kernel at 130-190 depending on the start address.
- The `ldm` kernel returns its last carry separately: added back into the 32-bit sum, it would be lost when the sum is
  0xFFFFFFFF.

## Fletcher and Adler-32

Fletcher-16, Fletcher-32, Fletcher-64 and Adler-32 share one kernel interface in `fletcher_arch.hpp`: `kernel<Bits>`
sums little-endian values of 8, 16 or 32 bits and returns, for a chunk of n values v_i, their sum and the weighted sum
of (n − i) · v_i. The caller adds n · `sum1` plus the weighted sum to `sum2` and reduces both sums after every chunk.
Fletcher-16 and Adler-32 run the same byte kernel with moduli 255 and 65521.

The portable loops are the baseline. They reduce only when an overflow could otherwise happen (every 380 million bytes
on 64-bit targets for Fletcher-16 and Adler-32) and add four blocks per step, so a byte loop still runs 7 300 MB/s on
x86-64 at 4 KiB with GCC, but only 0.84 GiB/s on a Cortex-A72 and 5.3 cycles per byte on a Cortex-M4.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX2 | Yes: bytes from 64 B, 16-bit values from 128 B, 32-bit values from 384 B | At 4 KiB with GCC, 7.4× the portable loop for Fletcher-16 and Adler-32, 3.3× for Fletcher-32, 2.0× for Fletcher-64. |
| x86-64 SSE2 (baseline), SSSE3 | Yes, bytes and 16-bit values | Fletcher-16 3.1× and Fletcher-32 1.5× the portable loop at 4 KiB with GCC. SSSE3 `pmaddubsw` weights 16 bytes in two instructions; plain SSE2 widens the bytes first. |
| x86-64 SSE2, 32-bit values | Not done | The 64-bit portable loop of Fletcher-64 already runs 26 900 MB/s at 4 KiB. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available. |
| AArch64 NEON | Yes: bytes from 64 B, 16-bit from 128 B, 32-bit from 256 B | Cortex-A72 at 4 KiB: about 5× for Fletcher-16 and Adler-32, 2.8× for Fletcher-32, 1.45× for Fletcher-64, whose 64-bit portable loop is already fast. |
| 32-bit Arm NEON, little-endian | Yes, the same kernels and thresholds | A72 in AArch32 at 4 KiB: 5.0× for Fletcher-16 and Adler-32, 2.9× for Fletcher-32, 3.0× for Fletcher-64. |
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
  inputs below the threshold run without a call.
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
