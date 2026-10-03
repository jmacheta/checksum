# CPU acceleration

How algorithms use CPU instructions, which architectures are worth it, and the CRC kernels as the worked example.
Measured figures are in the user guides ([CRC](../crc.md#8-performance)).

## Rules

- **Compile time only.** A private dispatch header (`checksum_private/crc_arch.hpp`) includes one architecture
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

## Which architectures are worth accelerating

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
