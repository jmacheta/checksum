# CPU acceleration

How algorithms use CPU instructions and which architectures are worth it, per algorithm. Measured figures are in the
user guides ([CRC](../crc.md#8-performance), [Internet checksum](../internet.md#5-performance)).

## Rules

- **Compile time only.** A private dispatch header per algorithm (`checksum_private/crc_arch.hpp`,
  `internet_arch.hpp`) includes one architecture
  header, chosen by the macros the compiler predefines for the flags of each translation unit (`__PCLMUL__`,
  `__ARM_FEATURE_CRC32`, `__riscv_zbc`, ...). A generic header covers every other target and the option
  `CHECKSUM_ACCELERATION=OFF`. No run-time CPU detection, no target attributes, no static state; the build never
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
on x86-64 that is 6 000 MB/s at 20 B and about 50 000 MB/s from 4 KiB, without any instruction-set flags. A kernel has
to beat that, not a byte loop.

| Target | Verdict | Why |
| --- | --- | --- |
| x86-64 AVX2 | Yes, from 256 B | 64-byte blocks, 32-bit halves summed into 64-bit lanes: 1.5-1.9× the portable loop at 1500 B-4 KiB, equal at 256 B, slower below. |
| x86-64 SSE2 (baseline) | No | The same scheme with 16-byte vectors matched the scalar carry loop and was never faster. |
| x86-64 AVX-512 | Not measured | No AVX-512 hardware available; the AVX2 kernel is near the L1 bandwidth, so the gain is likely small. |
| Compiler auto-vectorization | No | GCC `-O3 -march=native` vectorizes a 32-bit-word loop to about 47 000 MB/s, below the scalar carry loop; Clang does not vectorize it. |
| AArch64 NEON (`uadalp`) | Candidate | NEON is part of the AArch64 baseline, and pairwise add-accumulate takes 16 bytes per instruction. Needs a measurement on hardware against the portable loop, which compiles to `adds`/`adc`. |
| Cortex-M4 / ARMv7E-M | Candidate, assembly only | The portable loop compiles to about 21 instructions per 16 bytes; an `ldm` + `adcs` chain needs about 9. Neither compiler builtins (`__builtin_addc`) nor 64-bit accumulators reach that from C++, so it would be the library's first inline assembly. Measure before adding. |
| RISC-V | Not done | No carry flag: the portable loop costs three instructions per word. RVV could help; untested. |

Decisions, with the measurement behind each (Core Ultra 7 155H):

- Two carry chains of 64-bit words: 1.3× one chain at 64 B, 2.6× at 4 KiB. A four-chain prototype was up to 1.3×
  faster again with Clang but 4× slower with GCC, which kept its array in memory; untried with separate variables.
- The tail is loaded in fixed 4-, 2- and 1-byte steps: a `memcpy` of variable size called the library function and
  made a 20-byte input 3.5× slower.
- One final fold after the carry chains are merged: three separate folds cost 8-20 % at 20-64 B.
- The AVX2 kernel works in passes of 2^26 blocks (4 GiB), so its 64-bit lanes never overflow.
