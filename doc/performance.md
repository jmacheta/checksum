# Performance

Measured speed of every algorithm, with the default `isa::generic` next to the accelerated presets, and code size on
Cortex-M4. Every throughput figure comes from the benchmarks in [tests/benchmarks](../tests/benchmarks), and
[How to reproduce](#how-to-reproduce) gives the commands. The user guides explain the strategies, presets and kernels
named here.

## Summary

MB/s at 4 KiB with GCC: the fastest preset, and `isa::generic` in parentheses. Each row links to its section.

| Algorithm | x86-64, Core i7-13700K | Cortex-A72, AArch64 | Cortex-M4, STM32L4A6 at 80 MHz |
| --- | --- | --- | --- |
| [CRC-32](#crc) | 86 047 (3 408) | 11 057 (661) | 14.1 |
| [Internet checksum](#internet-checksum) | 118 423 (63 191) | 13 590 | 111.3 (62.2) |
| [Adler-32](#adler-32) | 110 073 (26 937) | 6 659 | 29.7 (16.7) |
| [Fletcher-16](#fletcher-16--32-and--64) | 111 240 (26 888) | 6 583 | 29.7 (16.7) |
| [fletcher4](#fletcher4) | 36 512 (24 703) | 3 036 | 25.3 |
| [XXH32](#xxhash) | 10 930 | 2 926 | 50.1 |
| [XXH3-64](#xxh3) | 72 946 (28 498) | 4 728 | 26.4 |
| [MurmurHash3 x86_32](#murmurhash3) | 5 288 | 1 187 | 32.4 |
| [FNV-1a 32](#fnv-1a) | 1 384 | 373 | 17.7 |

`x86_avx512` is not benchmarked: none of these machines has AVX-512, and its kernels are tested in Intel SDE only.
Where a single figure stands, no preset of that CPU beats `generic`. On AArch64 `generic` already has the NEON kernels,
and only CRC gains from `aarch64_crc`.

## How to reproduce

| Name used below | Hardware | Build | Run |
| --- | --- | --- | --- |
| x86-64 | Intel Core i7-13700K, one P-core, CachyOS | `native-gcc-bench` and `native-clang-bench` (GCC 16, Clang 23) | `taskset -c 2`, 5 repetitions |
| Cortex-A72 | Raspberry Pi 4 at 1.5 GHz, AArch64 and AArch32 | `cross-aarch64` and `cross-arm-crc32` (GCC 14.3) with benchmarks | `taskset -c 3`, `performance` governor, 5 repetitions |
| Cortex-M4 | STM32L4A6 (Nucleo-L4A6ZG); code in flash, data in RAM | `bare-cortex-m4` (GCC 14.3) | cycle counter, best of 5 runs of 8 calls |

Every build is Release at `-O2`. Each table reports the best of the repetitions in MB/s (10⁶ bytes per second), and
inputs are in the cache except where noted.

On x86-64, build and run one algorithm like this:

```sh
cmake --preset native-gcc-bench && cmake --build --preset native-gcc-bench
taskset -c 2 build/native-gcc-bench/tests/benchmarks/Release/checksum_bench_internet \
  --benchmark_filter='/4096$' --benchmark_repetitions=5 --benchmark_min_time=0.1s
```

The bench presets compile with `-march=native`, so `generic` here is the portable code as the compiler optimizes it for
this CPU; the kernels of the other presets do not depend on it. Benchmarks are named `<name>/<size>` for `isa::generic`,
`<name>_<preset>/<size>` for other presets and `<name>_dispatch/<size>` for the dispatch engine, which runs the best
preset of the CPU and costs up to 20 % on 20-byte inputs. `<name>` is the algorithm with its width (`fletcher16`,
`xxh3_64`) or the CRC strategy, preset and CRC (`sliced_x86_vpclmul/crc32_iso_hdlc/4096`). Fletcher and Adler-32 add
the start offset from an aligned address, 0 or 1 (`adler32/20/0`).

For the Cortex-A72, configure a cross preset with `-DCHECKSUM_BENCHMARKS=ON` and `-DCMAKE_CXX_FLAGS_RELEASE="-O2
-DNDEBUG"`, build the `checksum_bench_<algorithm>` targets in Release and copy the static executables to the board.
The `cross-aarch64` build uses `-march=armv8-a` and `cross-arm-crc32` uses `-march=armv8-a+crc -mfpu=neon-fp-armv8`.
The Pi 4 has no PMULL, so `aarch64_pmull` is not measured.

On a microcontroller, set the chip's memory layout and build the bare-metal preset:

```sh
CHECKSUM_LINK_FLAGS="-Wl,--section-start=.isr_vector=0x08000000 -Wl,-Ttext-segment=0x08001000 -Wl,-Tdata=0x20000000" \
  cmake --preset bare-cortex-m4 && cmake --build build/bare-cortex-m4 --config Release
```

Load `checksum_bench_<algorithm>.elf` with your debugger (code into flash, `.data` into RAM), enable semihosting and
start at `reset`. On the STM32L4A6, also set `FLASH_ACR` to 4 wait states with prefetch and both caches on (0x704):
without the caches every figure halves. Each benchmark prints its cycles per call and per byte, and MB/s is the clock
in MHz divided by cycles/B. The cycle counts belong to 4 wait states, the setting that 80 MHz needs, and the tables
convert them at 80 MHz. Sizes that do not fit the heap, like 1 MiB, are skipped.

Code placement moves short-input figures by up to 10 % on x86-64 and more on microcontrollers running from flash, so
differences of a few percent between builds that run the same code are noise.

## CRC

User guide: [crc.md](crc.md).

Strategies are named without their `crc_lut_` prefix.

### x86-64

GCC, MB/s at 16 B / 256 B / 4 KiB / 1 MiB; `none`, `nibble` and `byte` at 4 KiB. `x86_pclmul` runs 41 000-45 000 MB/s
from 256 B for every width.

| CRC | none | nibble | byte | sliced, `generic` | braided, `generic` | sliced, `x86_vpclmul` |
| --- | --- | --- | --- | --- | --- | --- |
| CRC-8/SMBUS | 214 | 297 | 925 | 3 989 / 4 162 / 3 643 / 3 630 | 3 965 / 5 557 / 7 276 / 7 406 | 8 777 / 53 818 / 84 256 / 87 677 |
| CRC-16/XMODEM | 212 | 286 | 909 | 3 985 / 3 395 / 3 111 / 3 082 | 3 800 / 5 399 / 7 128 / 7 272 | 8 778 / 53 820 / 84 146 / 87 679 |
| CRC-32/ISO-HDLC | 163 | 339 | 782 | 5 149 / 4 113 / 3 408 / 3 375 | 4 855 / 6 357 / 8 494 / 8 855 | 10 416 / 62 263 / 86 047 / 87 719 |
| CRC-64/XZ | 163 | 339 | 782 | 5 270 / 3 228 / 3 027 / 3 043 | 4 820 / 6 390 / 8 666 / 8 866 | 10 482 / 62 351 / 86 095 / 87 718 |

With a kernel, `braided` runs the same code as `sliced`. CRC-32C with `x86_vpclmul` runs 77 233 MB/s at 256 B. Clang
gives the same kernel figures, and its `sliced` portable loop is faster at 16 B (6 973 MB/s for CRC-32).

### Cortex-A72

MB/s at 16 B / 64 B / 256 B / 4 KiB / 1 MiB; `none`, `nibble` and `byte` at 4 KiB run 26-45, 75-93 and 213-299 MB/s.
The CRC32 instructions cover CRC-32 and CRC-32C only, so the other widths run the portable loop under every preset.

| CRC, strategy | AArch64 `aarch64_crc` | AArch64 `generic` | AArch32 `thumb2_crc` |
| --- | --- | --- | --- |
| CRC-32/ISO-HDLC, sliced | 956 / 2 896 / 7 079 / 11 057 / 6 261 | 523 / 622 / 653 / 661 / 652 | 853 / 2 030 / 3 982 / 5 706 / 4 300 |
| CRC-32/ISO-HDLC, braided | 824 / 2 655 / 6 826 / 10 999 / 6 284 | 468 / 605 / 775 / 846 / 854 | 747 / 1 911 / 3 823 / 5 686 / 4 274 |
| CRC-64/XZ, braided | | 459 / 597 / 581 / 821 / 793 | |
| CRC-16/KERMIT, braided | | 408 / 514 / 624 / 703 / 696 | |

At 1 MiB the input no longer fits the caches, which limits CRC-32 to about 6 300 MB/s.

### Cortex-M4

STM32L4A6, MB/s at 16 B / 4 KiB. Every column runs the portable loop, with the engine's tables in flash.

| CRC | none | nibble | byte | sliced | braided |
| --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC | 1.3 / 1.5 | 3.6 / 4.7 | 5.2 / 8.0 | 5.9 / 14.1 | 6.0 / 13.2 |
| CRC-32/BZIP2 | 1.3 / 1.5 | 3.6 / 5.0 | 5.3 / 8.9 | 5.9 / 15.1 | 5.6 / 13.3 |
| CRC-16/XMODEM | 1.1 / 1.2 | 3.2 / 4.2 | 5.0 / 8.0 | 5.6 / 13.8 | 5.5 / 13.7 |
| CRC-8/SMBUS | 1.2 / 1.3 | 3.3 / 4.4 | 6.0 / 11.4 | 6.0 / 15.9 | 5.9 / 13.2 |
| CRC-64/XZ | 0.7 / 0.8 | 2.4 / 3.0 | 3.7 / 5.3 | 4.3 / 8.6 | 4.2 / 4.6 |

### Code size

Footprint of one CRC through `crc_engine_for` on Cortex-M4, linked with `--gc-sections`: bytes of code added to an empty
program / bytes of engine in `.rodata`.

| CRC | none | nibble | byte | sliced | braided |
| --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC | 100 / 32 | 104 / 96 | 80 / 1 056 | 252 / 8 400 | 1 116 / 16 592 |
| CRC-16/XMODEM | 104 / 16 | 104 / 48 | 76 / 528 | 252 / 4 288 | 1 212 / 8 384 |
| CRC-8/SMBUS | 101 / 11 | 101 / 27 | 97 / 267 | 252 / 2 240 | 1 068 / 4 288 |
| CRC-64/XZ | 116 / 64 | 140 / 192 | 100 / 2 112 | 380 / 16 608 | 1 868 / 32 992 |

## Internet checksum

User guide: [internet.md](internet.md).

### x86-64

Below 512 B every preset runs the portable loop. `x86_avxvnni` runs the `x86_avx2` kernel.

| Input | GCC `generic` | GCC `x86_avx2` | Clang `generic` | Clang `x86_avx2` |
| --- | --- | --- | --- | --- |
| 20 B (IPv4 header) | 9 971 | 9 530 | 9 142 | 8 775 |
| 64 B | 29 252 | 28 083 | 28 087 | 27 006 |
| 256 B | 53 937 | 53 959 | 52 348 | 51 991 |
| 1500 B (Ethernet payload) | 59 076 | 105 074 | 62 386 | 115 372 |
| 4 KiB | 63 191 | 118 423 | 63 999 | 126 702 |
| 1 MiB | 69 670 | 129 254 | 69 674 | 126 581 |

### Cortex-A72

GCC. On AArch64 the NEON kernel is part of `generic` and starts at 512 B; on AArch32 the kernels start at 192 B.

| Input | AArch64 `generic` | AArch32 `generic` | AArch32 `thumb2` | AArch32 `thumb2_neon` |
| --- | --- | --- | --- | --- |
| 20 B | 1 149 | 766 | 681 | 681 |
| 64 B | 2 808 | 1 686 | 1 558 | 1 567 |
| 256 B | 5 539 | 2 673 | 3 212 | 4 376 |
| 1500 B | 10 924 | 3 177 | 4 686 | 9 483 |
| 4 KiB | 13 588 | 3 308 | 5 354 | 11 804 |
| 1 MiB | 7 010 | 3 037 | 4 684 | 6 612 |

### Cortex-M4

STM32L4A6, MB/s. Below 192 B `thumb2` runs the portable loop.

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| `generic` | 9.4 | 22.3 | 43.6 | 59.5 | 62.2 |
| `thumb2` | 9.3 | 22.0 | 48.3 | 94.4 | 111.3 |

### Code size

`sum_loop` and its helpers, GCC `-ffunction-sections`. A kernel build holds the portable loop twice, for short inputs
and for the tail after the kernel. RV64 without fast unaligned access loads words byte by byte.

| Target | Portable | With kernel |
| --- | --- | --- |
| Cortex-M4, `-Os` | 284 B | 920 B |
| Cortex-M4, `-O2` | 356 B | 1 104 B |
| x86-64, `-O2` (AVX2 with `-mavx2`) | 385 B | 985 B |
| AArch64, `-O2` | 484 B | 1 076 B |
| RISC-V RV64, `-O2` (V with `-march=rv64gcv`) | 1 620 B | 1 402 B |

## Adler-32

User guide: [adler32.md](adler32.md).

### x86-64

GCC, aligned start; `x86_vpclmul` runs the `x86_avx2` kernel.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| `generic` | 4 926 | 9 693 | 20 566 | 24 352 | 26 937 | 27 695 |
| `x86_avx2` | 4 927 | 10 683 | 29 833 | 51 034 | 68 376 | 73 452 |
| `x86_avxvnni` | 4 927 | 12 163 | 38 362 | 68 527 | 110 073 | 116 117 |

From an odd address, `x86_avxvnni` runs 92 246 MB/s at 4 KiB. Clang's `generic` loop is slower, 19 390 MB/s at 4 KiB,
and so is its `x86_avxvnni`, 103 284.

### Cortex-A72

GCC, aligned start. On AArch64 the NEON kernel is part of `generic`.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| AArch64 `generic` | 574 | 1 137 | 2 637 | 5 399 | 6 659 | 5 439 |
| AArch32 `generic` | 436 | 693 | 800 | 899 | 915 | 948 |
| AArch32 `thumb2_neon` | 430 | 1 074 | 2 583 | 4 039 | 4 667 | 3 956 |

### Cortex-M4

STM32L4A6, MB/s; from an odd address `thumb2_dsp` runs 29.5 MB/s at 4 KiB.

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `generic` | 5.9 | 10.5 | 13.0 | 14.0 | 14.6 | 16.4 | 16.7 |
| `thumb2_dsp` | 5.6 | 11.1 | 16.3 | 19.3 | 21.2 | 28.0 | 29.7 |

### Code size

Cortex-M4, GCC 14.3: the out-of-line loop with the DSP kernel takes 820 B of code at `-O2` and 662 B at `-Os`;
the inline short paths of the header add to each caller.

## Fletcher-16, -32 and -64

User guide: [fletcher.md](fletcher.md).

### x86-64

MB/s at 4 KiB, aligned start. `generic` runs SSE2 kernels for Fletcher-16 and -32 and the portable loop for
Fletcher-64.

| Checksum | GCC `generic` | GCC `x86_avx2` | GCC `x86_avxvnni` | Clang `generic` | Clang `x86_avxvnni` |
| --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 26 888 | 67 396 | 111 240 | 19 340 | 104 432 |
| Fletcher-32 | 33 221 | 61 224 | 61 605 | 33 377 | 64 083 |
| Fletcher-64 | 30 547 | 63 720 | 63 920 | 17 105 | 65 825 |

`x86_avxvnni`, GCC, MB/s:

| Checksum | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 4 742 | 11 807 | 36 381 | 68 324 | 111 240 | 116 484 |
| Fletcher-32 | 5 765 | 10 907 | 25 476 | 48 704 | 61 605 | 61 396 |
| Fletcher-64 | 5 788 | 13 746 | 24 143 | 52 980 | 63 920 | 71 608 |

### Cortex-A72

GCC, aligned start, MB/s at 64 B / 256 B / 1500 B / 4 KiB. On AArch64 the NEON kernels are part of `generic`.

| Checksum | AArch64 `generic` | AArch32 `generic` | AArch32 `thumb2_neon` |
| --- | --- | --- | --- |
| Fletcher-16 | 1 007 / 2 453 / 5 270 / 6 583 | 564 / 780 / 917 / 935 | 1 017 / 2 498 / 3 784 / 4 683 |
| Fletcher-32 | 1 257 / 2 914 / 4 766 / 5 272 | 885 / 1 554 / 1 723 / 1 765 | 928 / 2 565 / 4 379 / 5 127 |
| Fletcher-64 | 1 706 / 3 054 / 4 811 / 5 638 | 893 / 1 421 / 1 728 / 1 797 | 838 / 2 323 / 4 348 / 5 464 |

### Cortex-M4

STM32L4A6, MB/s at 4 KiB. Fletcher-32 and -64 have no DSP kernel and run the portable loop under every preset.

| Checksum | `generic`, aligned | `generic`, odd start | `thumb2_dsp`, aligned | `thumb2_dsp`, odd start |
| --- | --- | --- | --- | --- |
| Fletcher-16 | 16.7 | 16.7 | 29.7 | 29.5 |
| Fletcher-32 | 39.7 | 31.8 | no kernel | no kernel |
| Fletcher-64 | 45.6 | 35.5 | no kernel | no kernel |

Fletcher-16, MB/s:

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `generic` | 6.1 | 10.8 | 13.2 | 14.1 | 14.8 | 16.4 | 16.7 |
| `thumb2_dsp` | 5.8 | 11.3 | 16.4 | 19.4 | 21.4 | 28.1 | 29.7 |

On this core the wider sums are cheaper per byte, and Fletcher-32 and -64 are slower from an odd address.

### Code size

Cortex-M4, GCC 14.3: the out-of-line loops of the three widths take 2 024 B of code at `-O2` and 1 802 B at `-Os`; the
inline short paths of the header add to each caller.

## fletcher4

User guide: [fletcher4.md](fletcher4.md).

### x86-64

MB/s. Below the kernel thresholds every preset runs the word loop. `x86_avxvnni` runs the `x86_avx2` kernel.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| GCC `generic` | 8 397 | 13 203 | 18 142 | 23 296 | 24 703 | 25 540 |
| GCC `x86_avx2` | 8 346 | 13 149 | 24 239 | 32 766 | 36 368 | 38 341 |
| Clang `generic` | 7 791 | 15 381 | 18 993 | 23 793 | 24 915 | 25 487 |
| Clang `x86_avx2` | 7 380 | 15 354 | 23 346 | 35 139 | 37 902 | 38 717 |

### Cortex-A72

GCC, MB/s. On AArch64 the NEON kernel is part of `generic` and starts at 192 B.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| AArch64 `generic` | 1 106 | 1 620 | 2 277 | 2 874 | 3 036 | 2 804 |
| AArch32 `generic` | 510 | 770 | 943 | 1 057 | 1 076 | 1 056 |
| AArch32 `thumb2_neon` | 476 | 769 | 1 138 | 2 060 | 2 397 | 2 352 |

### Cortex-M4

STM32L4A6, word loop under every preset. MB/s:

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| `generic` | 7.3 | 14.2 | 21.3 | 24.8 | 25.3 |

### Code size

Cortex-M4, GCC 14.3: the out-of-line word loop and its callers take 616 B of code at `-O2` and 654 B at `-Os`; the
inline short paths of the header add to each caller.

## xxHash

XXH32 and XXH64. User guide: [xxhash.md](xxhash.md). Every preset runs the same portable code.

| Hash, platform | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| XXH32, x86-64 GCC | 8 247 | 10 325 | 10 802 | 10 940 | 10 930 | 10 972 |
| XXH32, x86-64 Clang | 5 859 | 11 498 | 11 553 | 7 406 | 6 942 | 6 677 |
| XXH64, x86-64 GCC | 9 400 | 11 600 | 18 002 | 20 925 | 21 638 | 21 940 |
| XXH32, AArch64 | 786 | 1 737 | 2 532 | 2 800 | 2 926 | 2 668 |
| XXH64, AArch64 | 814 | 937 | 1 554 | 1 876 | 1 956 | 1 910 |
| XXH32, AArch32 | 635 | 1 469 | 2 374 | 2 765 | 2 910 | 2 613 |
| XXH64, AArch32 | 282 | 372 | 596 | 701 | 734 | 730 |
| XXH32, Cortex-M4 | 8.4 | 20.9 | 37.6 | 47.8 | 50.1 | |
| XXH64, Cortex-M4 | 7.5 | 6.0 | 13.8 | 21.0 | 23.1 | |

XXH64 with Clang runs within 4 % of GCC. On the Cortex-M4, XXH32 takes 1.60 cycles per byte at 4 KiB and XXH64 3.47.

### Code size

Cortex-M4, GCC 14.3: the out-of-line loops take 2 672 B of code at `-O2` and 1 720 B at `-Os`; the inline short paths
of the header add to each caller.

## XXH3

User guide: [xxh3.md](xxh3.md).

### x86-64

XXH3-64, MB/s. Up to 240 B every build runs straight-line code; from 241 B XXH3 switches to the stripe loop, which
`generic` runs in SSE2. `x86_avxvnni` runs the `x86_avx2` kernel.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| GCC `generic` | 7 803 | 17 545 | 16 966 | 25 695 | 28 498 | 28 583 |
| GCC `x86_avx2` | 9 263 | 20 599 | 38 804 | 65 803 | 72 942 | 73 430 |
| Clang `generic` | 13 674 | 28 020 | 31 860 | 38 625 | 40 436 | 40 301 |
| Clang `x86_avx2` | 13 687 | 28 036 | 38 104 | 66 620 | 73 471 | 74 482 |

XXH3-128 with GCC `x86_avx2` runs 69 695 MB/s at 4 KiB.

### Cortex-A72

GCC, MB/s. On AArch64 the NEON kernel is part of `generic`.

| Hash, build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| XXH3-64, AArch64 `generic` | 886 | 1 911 | 2 795 | 4 345 | 4 728 | 4 319 |
| XXH3-128, AArch64 `generic` | 664 | 1 426 | 2 066 | 3 972 | 4 555 | 4 274 |
| XXH3-64, AArch32 `generic` | 403 | 834 | 904 | 1 250 | 1 344 | 1 321 |
| XXH3-64, AArch32 `thumb2_neon` | 399 | 820 | 1 419 | 2 486 | 2 818 | 2 689 |
| XXH3-128, AArch32 `thumb2_neon` | 318 | 683 | 1 087 | 2 268 | 2 689 | 2 707 |

### Cortex-M4

STM32L4A6, portable code under every preset. MB/s, and cycles per byte at 4 KiB:

| Hash | 20 B | 64 B | 200 B | 256 B | 1500 B | 4 KiB | cycles/B |
| --- | --- | --- | --- | --- | --- | --- | --- |
| XXH3-64 | 5.8 | 13.1 | 16.7 | 12.7 | 23.0 | 26.4 | 3.03 |
| XXH3-128 | 5.1 | 11.4 | 11.4 | 10.9 | 21.8 | 25.8 | 3.10 |

XXH3 hashes 17-128 B, 129-240 B and longer inputs with different code, so throughput changes at those boundaries.

### Code size

Cortex-M4, GCC 14.3: the out-of-line loops take 976 B of code at `-O2` and 576 B at `-Os`; the inline short paths of
the header add to each caller.

## MurmurHash3

User guide: [murmur3.md](murmur3.md). Every preset runs the same portable code.

| Hash, platform | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| x86_32, x86-64 GCC | 6 487 | 7 119 | 6 414 | 5 382 | 5 288 | 5 225 |
| x64_128, x86-64 GCC | 6 329 | 10 148 | 11 916 | 12 477 | 12 158 | 12 210 |
| x86_32, AArch64 | 708 | 985 | 1 086 | 1 175 | 1 187 | 1 160 |
| x64_128, AArch64 | 533 | 995 | 1 421 | 1 637 | 1 676 | 1 635 |
| x86_32, AArch32 | 597 | 915 | 1 071 | 1 123 | 1 135 | 1 122 |
| x64_128, AArch32 | 271 | 405 | 539 | 598 | 605 | 603 |
| x86_32, Cortex-M4 | 8.3 | 17.4 | 26.9 | 31.5 | 32.4 | |
| x64_128, Cortex-M4 | 5.8 | 9.9 | 14.9 | 17.7 | 18.0 | |

Clang is about 20 % slower on x86-64: 4 243 and 9 094 MB/s at 4 KiB.

### Code size

Cortex-M4, GCC 14.3: the block loops of both variants and the one-shot x86_32 function take 900 B of code at `-O2`
and 748 B at `-Os`; the inline short paths of the header add to each caller.

## FNV-1a

User guide: [fnv1a.md](fnv1a.md). Every preset runs the same portable code.

| Hash, platform | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| FNV-1a 32, x86-64 GCC | 4 116 | 2 564 | 1 667 | 1 410 | 1 384 | 1 368 |
| FNV-1a 64, x86-64 GCC | 4 250 | 2 588 | 1 666 | 1 410 | 1 383 | 1 368 |
| FNV-1a 32, AArch64 | 476 | 400 | 374 | 373 | 373 | 372 |
| FNV-1a 64, AArch64 | 343 | 282 | 294 | 298 | 298 | 296 |
| FNV-1a 32, AArch32 | 459 | 397 | 374 | 373 | 373 | 371 |
| FNV-1a 64, AArch32 | 355 | 368 | 367 | 372 | 373 | 370 |
| FNV-1a 32, Cortex-M4 | 10.5 | 14.6 | 16.9 | 17.6 | 17.7 | |
| FNV-1a 64, Cortex-M4 | 6.6 | 8.2 | 8.9 | 9.1 | 9.1 | |

Each byte waits for the multiplication of the previous one: about 4 cycles per byte on x86-64 and the Cortex-A72. The
A72's 64-bit multiplication is slower, so from 64 bytes FNV-1a 64 in AArch64 multiplies 32-bit halves, as AArch32 does:
5 cycles per byte instead of 6. Below 64 bytes the single multiplication stays, since it hashes many short inputs in a
row faster. On the Cortex-M4, FNV-1a 32 takes 4.51 cycles per byte at 4 KiB and FNV-1a 64 8.76.

### Code size

Cortex-M4, GCC 14.3: the inline loop takes 132 B of code for FNV-1a 32 and 188 B for FNV-1a 64 at `-O2`, 100 B and
156 B at `-Os`.
