# Performance

Measured speed and code size of every algorithm, on desktop, single-board and microcontroller CPUs. The user guides
explain the strategies and CPU kernels these figures refer to; this page only measures them.

## How to read the figures

- **Speed is in MB/s**, 10⁶ bytes per second. Higher is better.
- **Inputs are already in the cache**, except at 1 MiB on the Cortex-A72, whose cache is smaller.
- **Short inputs measure throughput, not latency.** The benchmarks hash the same buffer in independent calls, which
  the CPU overlaps. When each call waits for the previous result, short inputs are slower: MurmurHash3_x86_32 at 64 B
  drops from 4 930 to 3 130 MB/s.
- **Portable** is the library without CPU kernels (`CHECKSUM_ACCELERATION=OFF`, or a CPU without the instructions).
  **SSE2**, **AVX2**, **NEON**, **DSP** and so on name the kernel that ran.
- **Comparisons with other libraries** (zlib-ng, Intel ISA-L, OpenZFS, xxHash, Linux, DPDK, SMHasher) were built with
  the same compiler and flags and called the same way. A ratio above 1.0× means this library is faster.
- **Code placement moves short-input figures** by up to 10 % on x86-64 and up to 25 % on microcontrollers running
  from flash. Differences below that between two builds that run the same code are noise.

To measure on your machine, run the benchmark presets: `cmake --workflow --preset native-gcc-bench` (or
`native-clang-bench`, and the `-portable` variants). The benchmarks are in `tests/benchmarks`.

## Platforms

| Name used below | Hardware | Compilers | Measured with |
| --- | --- | --- | --- |
| x86-64 | Intel Core Ultra 7 155H (laptop) | GCC 16, Clang 21, `-O2` | Google Benchmark, one pinned core |
| Cortex-A72 | Raspberry Pi 4, 1.5 GHz, AArch64 and AArch32 | GCC 14.3, Clang 21, `-O2` | Google Benchmark, one core |
| Cortex-M4 | nRF52840 at 64 MHz and STM32L4A6 at 80 MHz; code in flash, data in RAM | GCC 14.3, `-O2` | cycle counter, best of 5 calls |
| Cortex-M33 | nRF54L15 at 128 MHz (with DSP and FPU); code in RRAM with the 8 KiB instruction cache on, data in RAM | GCC 14.3, `-O2 -mcpu=cortex-m33` | cycle counter, best of 5 calls |

Both Cortex-M4 chips do the same work per clock cycle, so their speed scales with the clock: a figure measured at
64 MHz is 1.25× higher at 80 MHz. The nRF54L15 scales the same way down to 64 MHz; with its instruction cache off, every
loop is 3-4× slower. RISC-V has not been measured on
hardware: its kernels are tested in QEMU only.

## CRC

User guide: [crc.md](crc.md).

Strategies are named without their `crc_lut_` prefix. Portable means built with `CHECKSUM_ACCELERATION=OFF` or for a
CPU without the instructions listed in [crc.md](crc.md#7-cpu-acceleration).

### x86-64

GCC 16. Accelerated = `-march=native` (PCLMULQDQ, VPCLMULQDQ, AVX2, SSE4.2). MB/s at 16 B / 256 B / 4 KiB / 1 MiB;
`none`, `nibble` and `byte` at 4 KiB, where they have reached their steady speed.

| CRC | none | nibble | byte | sliced, portable | braided, portable | sliced, accelerated |
| --- | --- | --- | --- | --- | --- | --- |
| CRC-8/SMBUS | 180 | 252 | 797 | 3814 / 3540 / 3091 / 3019 | 3705 / 4940 / 5976 / 6045 | 10564 / 49017 / 71769 / 74200 |
| CRC-16/XMODEM | 176 | 245 | 781 | 4099 / 3012 / 2579 / 2589 | 3664 / 4703 / 5884 / 6009 | 10573 / 48774 / 72463 / 71670 |
| CRC-32/ISO-HDLC | 137 | 289 | 673 | 4663 / 3657 / 2923 / 2896 | 4332 / 5345 / 6685 / 6556 | 9754 / 52967 / 74320 / 75889 |
| CRC-64/XZ | 137 | 287 | 661 | 4758 / 2761 / 2501 / 2424 | 4456 / 5192 / 6710 / 6485 | 10291 / 53467 / 73653 / 75693 |

The accelerated `crc_lut_braided` is within 6 % of the accelerated `crc_lut_sliced`. Clang gives the same picture.
Against Intel ISA-L (hand-written AVX2 VPCLMULQDQ assembly), the accelerated CRC-32, CRC-32C and CRC-64/XZ run at
0.89-1.45× its speed from 20 B to 1 MiB with GCC 16, and 0.95-1.01× at 256 B.

### Cortex-A72

GCC 14, `-march=armv8-a+crc`: this core has the CRC extension but no PMULL, so only CRC-32 and CRC-32C are
accelerated. MB/s at 16 B / 64 B / 4 KiB.

| CRC, strategy | accelerated | portable |
| --- | --- | --- |
| CRC-32/ISO-HDLC, sliced | 1256 / 3812 / 11167 | 569 / 634 / 655 |
| CRC-32/ISO-HDLC, braided | 1138 / 3286 / 11060 | 494 / 623 / 848 |
| CRC-64/XZ, sliced / braided | | 537 / 623 / 655, 526 / 623 / 827 |
| CRC-16/KERMIT, sliced / braided | | 472 / 537 / 569, 451 / 526 / 698 |
| any width, byte | | 215 / 215 / 215 |

Against zlib-ng and Intel ISA-L built with the same flags (`-O2 -mcpu=cortex-a72+crc`), CRC-32 runs at 1.20× the
faster of the two at 20 B, 0.94× at 256 B and 0.97-1.00× from 4 KiB; CRC-32C at 1.00-1.13× of ISA-L and Google
crc32c. Without PMULL, CRC-64/XZ runs the sliced loop at 1.9-4.5× ISA-L's byte table. At 1 MiB every implementation
drops to about 6 000 MB/s against about 10 900 at 4 KiB, because the input no longer fits in the cache.

### Cortex-M4

GCC 14 `-O2`, instruction cache on, MB/s at 1 KiB with the engine in flash / in RAM. ARMv7E-M has no CRC or
carry-less multiplication instructions, so every build is portable.

| CRC | none | nibble | byte | sliced | braided |
| --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC | 1.14 | 2.95 / 3.75 | 4.92 / 6.36 | 7.77 / 12.3 | 7.05 / 10.5 |
| CRC-32/BZIP2 | 1.33 | 3.22 / 4.24 | 5.31 / 7.06 | 7.42 / 11.2 | 6.96 / 10.6 |
| CRC-16/XMODEM | 1.00 | 3.10 / 3.75 | 4.92 / 6.35 | 7.31 / 10.9 | 7.07 / 10.8 |
| CRC-8/SMBUS | 1.00 | 3.42 / 3.54 | 5.86 / 9.04 | 8.00 / 12.6 | 6.76 / 10.5 |

`crc_lut_sliced` is the fastest strategy on this core; `crc_lut_braided` is slower and twice the size. Code
placement in flash changes the table loops by several percent.

Footprint (`-ffunction-sections -fdata-sections --gc-sections`, a program computing one CRC with `crc_engine_for`):
code added over an empty program / engine in `.rodata`, bytes.

| CRC | none | nibble | byte | sliced | braided |
| --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC | 100 / 32 | 104 / 96 | 80 / 1056 | 252 / 8400 | 1116 / 16592 |
| CRC-16/XMODEM | 104 / 16 | 104 / 48 | 76 / 528 | 252 / 4288 | 1212 / 8384 |
| CRC-8/SMBUS | 101 / 11 | 101 / 27 | 97 / 267 | 252 / 2240 | 1068 / 4288 |
| CRC-64/XZ | 116 / 64 | 140 / 192 | 100 / 2112 | 380 / 16608 | 1868 / 32992 |

### Cortex-M4 with a CRC peripheral

The STM32 CRC unit driven by a custom strategy like `examples/crc/hardware_strategy.cpp` (8-bit writes), next to
the built-in strategies with the engine in RAM. MB/s at 16 B / 1 KiB.

| CRC | CRC unit | `crc_lut_sliced` | `crc_lut_byte` |
| --- | --- | --- | --- |
| CRC-32/MPEG-2 | 9.7 / 15.8 | 6.5 / 14.0 | 5.9 / 8.8 |
| CRC-32/ISO-HDLC | 3.3 / 15.1 | 7.1 / 15.3 | 5.7 / 8.0 |
| CRC-16/XMODEM | 9.7 / 15.8 | 6.3 / 13.7 | 5.4 / 7.9 |
| CRC-8/SMBUS | 9.7 / 15.8 | 6.8 / 15.7 | 6.7 / 11.3 |

Fed byte by byte, the peripheral is about as fast as `crc_lut_sliced` in RAM on long messages, but needs no table.
Every call configures the unit, and for a reflected CRC the example reverses the state bitwise in software, which
dominates short inputs (CRC-32/ISO-HDLC at 16 B).

### Cortex-M33

MB/s at 16 B / 1 KiB with the engine in RRAM (`crc_engine_for`, read-only data) and copied to RAM.

| CRC | none | nibble | byte | sliced | braided |
| --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC, RRAM | 2.3 / 2.4 | 6.8 / 7.5 | 11.1 / 12.8 | 13.2 / 18.5 | 13.6 / 13.1 |
| CRC-32/ISO-HDLC, RAM | 2.3 / 2.4 | 6.6 / 7.5 | 10.5 / 12.8 | 14.2 / 25.9 | 14.0 / 23.4 |
| CRC-32/BZIP2, RAM | 2.3 / 2.4 | 6.8 / 8.0 | 10.1 / 12.7 | 13.3 / 25.2 | 13.3 / 23.7 |
| CRC-16/XMODEM, RAM | 2.0 / 2.1 | 6.1 / 7.1 | 9.1 / 11.6 | 11.9 / 25.1 | 11.9 / 23.2 |
| CRC-8/SMBUS, RAM | 2.2 / 2.4 | 6.1 / 7.1 | 14.5 / 21.2 | 13.7 / 28.8 | 13.6 / 24.0 |

The instruction cache does not hold data, so table lookups in RRAM wait for it: in RAM `crc_lut_sliced` is 1.2-1.4×
and `crc_lut_braided` 1.2-1.9× faster at 1 KiB. Clock for clock the sliced loop in RAM matches the
Cortex-M4: CRC-32 runs 25.9 MB/s at 128 MHz, the nRF52840 12.3 MB/s at 64 MHz. The chip has no general-purpose CRC unit: the CRC logic of its radio and NFC peripherals
covers only their own frames.

## Internet checksum

User guide: [internet.md](internet.md).

### x86-64

AVX2 is with `-march=native`; portable is with `CHECKSUM_ACCELERATION=OFF` and default flags.

| Input | GCC 16 portable | GCC 16 AVX2 | Clang 21 portable | Clang 21 AVX2 |
| --- | --- | --- | --- | --- |
| 20 B (IPv4 header) | 8 198 | 8 193 | 7 564 | 7 573 |
| 64 B | 25 213 | 24 161 | 24 291 | 23 353 |
| 256 B | 46 748 | 46 352 | 45 065 | 45 027 |
| 1500 B (Ethernet payload) | 53 538 | 74 313 | 52 868 | 81 358 |
| 4 KiB | 55 492 | 82 858 | 56 057 | 95 187 |
| 1 MiB | 59 974 | 71 025 | 60 008 | 88 316 |

Below 512 bytes both builds run the portable loop; the differences come from `-march=native` and code layout. From
1500 bytes the AVX2 kernel is 1.4-1.7× faster up to 4 KiB, and 1.2× (GCC) or 1.5× (Clang) at 1 MiB.

Against Linux v7.3-rc5 `csum_partial` and DPDK `rte_raw_cksum`, built with the same GCC 16 `-O2 -march=native` and
called the same way, this build runs at 0.97× the faster of the two at 256 B, 0.98× at 64 B, 1.14× at 20 B and
1.35-1.76× from 1500 B.

### Cortex-A72

GCC 14.3, `-O2`, static binaries on one core. Portable is with `CHECKSUM_ACCELERATION=OFF`. AArch64 is
`-march=armv8-a`; in 32-bit mode, NEON is `-march=armv8-a+crc -mfpu=neon-fp-armv8`, and `ldm` is `-mfpu=vfpv3-d16`
(no NEON).

| Input | AArch64 portable | AArch64 NEON | AArch32 portable | AArch32 NEON | AArch32 `ldm` |
| --- | --- | --- | --- | --- | --- |
| 20 B | 1 190 | 1 144 | 899 | 842 | 837 |
| 64 B | 2 974 | 2 758 | 1 779 | 1 730 | 1 730 |
| 256 B | 5 682 | 5 437 | 2 819 | 4 426 | 3 536 |
| 1500 B | 7 288 | 11 204 | 3 186 | 9 409 | 4 838 |
| 4 KiB | 7 527 | 13 620 | 3 309 | 11 777 | 5 413 |
| 1 MiB | 5 454 | 6 975 | 3 032 | 6 777 | 4 679 |

The 64-bit portable loop is already fast, so the NEON kernel starts at 512 bytes and is 1.5-1.8× faster at
1500 B-4 KiB (1.28× at 1 MiB). In 32-bit mode the kernels start at 192 bytes: NEON is 3.6× and `ldm` 1.6× faster at
4 KiB. Below the thresholds every build runs the portable loop, and the differences there come from code layout.

Against Linux v7.3-rc5 `do_csum` in a separate comparison harness, AArch64 is 1.10× faster at 20 B (957 MB/s there;
the harness gives lower figures than this table). `do_csum` reads whole 8-byte words and masks off the bytes past the
end.

### Cortex-M4

Offset is the start
address modulo 4: 2 is typical for an IP header behind a 14-byte Ethernet header.

| Input | nRF52840 portable | nRF52840 `ldm` | STM32L4A6 portable | STM32L4A6 `ldm` |
| --- | --- | --- | --- | --- |
| 20 B | 10.8 | 10.5 | 13.4 | 13.1 |
| 64 B | 23.3 | 22.6 | 29.1 | 28.3 |
| 256 B | 39.4 | 52.0 | 49.2 | 65.0 |
| 1500 B | 48.9 | 82.6 | 61.1 | 103.4 |
| 4 KiB | 50.3 | 92.6 | 62.8 | 115.7 |
| 1500 B, offset 2 | 41.0 | 81.1 | 51.3 | 101.9 |
| 4 KiB, offset 1 | 36.1 | 90.2 | 45.1 | 112.8 |

The kernel is 1.8× faster at 4 KiB, 1.3× at 256 bytes, and up to 2.5× from odd or 2-modulo-4 addresses, where the
portable loop's unaligned loads cost more. Below 192 bytes both builds run the portable loop. The kernel breaks even at
120-160 bytes, depending on the start address, so the threshold could move lower.

### Cortex-M33

MB/s:

| Input | Portable | `ldm` |
| --- | --- | --- |
| 20 B | 26.4 | 27.2 |
| 64 B | 57.7 | 58.1 |
| 256 B | 101.8 | 118.7 |
| 1500 B | 129.7 | 184.3 |
| 4 KiB | 133.7 | 205.1 |
| 1500 B, offset 2 | 103.5 | 181.6 |
| 4 KiB, offset 2 | 106.0 | 201.6 |

At 4 KiB the kernel is 1.55× faster than the portable loop. From an aligned
address it breaks even at about 96 bytes, but at 64 bytes from an odd or 2-modulo-4 address it is 30 % slower than
the portable loop, so the 192-byte threshold stays.

### Code size

`sum_loop` and its helpers, the whole run-time code, GCC with `-ffunction-sections`. A kernel build holds the portable
loop twice: once for short inputs, once after the kernel.

| Target | Portable | With kernel |
| --- | --- | --- |
| Cortex-M4, `-Os` | 284 B | 920 B |
| Cortex-M4, `-O2` | 356 B | 1 104 B |
| x86-64, `-O2` (AVX2 with `-mavx2`) | 385 B | 985 B |
| AArch64, `-O2` | 484 B | 1 076 B |
| RISC-V RV64, `-O2` (V with `-march=rv64gcv`) | 1 620 B | 1 402 B |

RV64 without fast unaligned access loads each word byte by byte, which makes its unrolled portable loop large.

## Adler-32

User guide: [adler32.md](adler32.md).

### x86-64

GCC 16, 4 KiB, one pinned core, the better median of two runs: 7 309 MB/s for the portable
loop, 22 964 MB/s with SSE2 and 58 088 MB/s with the AVX2 kernel, 7.9× faster. With `-march=native` the AVX-VNNI
kernel runs (MB/s, GCC 16 and Clang 21):

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| GCC | 4 074 | 11 570 | 35 225 | 56 789 | 84 504 | 88 630 |
| Clang | 3 746 | 11 655 | 36 649 | 57 952 | 85 027 | 89 914 |

In a separate comparison harness, against zlib-ng (develop, its AVX-VNNI kernel, same flags) the library runs 0.98×
at 20 B, 0.78× at 64 B, 0.91× at 256 B and 1.13-1.17× from 4 KiB; the AVX2 kernel reached 0.71-0.83× there.

### Cortex-A72

GCC 14.3, `-O2`, AArch64 `-march=armv8-a`, one core, start address aligned. MB/s:

| Build | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- |
| AArch64 portable | 740 | 830 | 890 | 900 |
| AArch64 NEON | 1 230 | 2 790 | 5 380 | 6 610 |
| AArch32 portable | 710 | 830 | 910 | 930 |
| AArch32 NEON | 1 020 | 2 600 | 4 030 | 4 620 |

NEON is 5.0-7.3× faster at 4 KiB and 1.4-1.7× at 64 bytes. On AArch64, from 320 bytes the kernel sums 64 bytes per
iteration: 0.96-0.99× zlib-ng and ISA-L from 1500 B in the comparison harness, where 16 bytes per iteration reached
0.72-0.79×.

### Cortex-M4

STM32L4A6 at 80 MHz, MB/s; from an odd address the DSP kernel runs
29.6 MB/s at 4 KiB:

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Portable | 6.6 | 10.7 | 12.6 | 13.4 | 13.8 | 14.9 | 15.1 |
| DSP kernel | 6.8 | 12.9 | 18.1 | 20.9 | 22.7 | 28.5 | 29.8 |

The kernel is 2.0× faster at 4 KiB and 1.6× at 256 bytes.

### Cortex-M33

MB/s; from an odd address the DSP kernel runs 47.8 MB/s at 4 KiB:

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| Portable | 13.8 | 21.0 | 26.1 | 28.0 | 28.3 |
| DSP kernel | 13.8 | 22.8 | 38.0 | 46.0 | 47.9 |

### Code size

Cortex-M4, GCC 14.3: the out-of-line loop with the DSP kernel takes 820 B of code at `-O2` and 662 B at `-Os`;
the inline short paths of the header add to each caller.

## Fletcher-16, -32 and -64

User guide: [fletcher.md](fletcher.md).

### x86-64

MB/s at 4 KiB, GCC 16 and Clang 21, one pinned core, the better median of two runs.
Portable is the library without kernels, SSE2 the default x86-64 flags, AVX2 a build with AVX2 enabled.

| Checksum | GCC portable | GCC SSE2 | GCC AVX2 | Clang portable | Clang AVX2 |
| --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 7 373 | 22 955 | 54 620 | 3 183 | 56 748 |
| Fletcher-32 | 14 419 | 21 918 | 53 037 | 6 436 | 52 933 |
| Fletcher-64 | 27 485 | | 51 094 | 12 977 | 53 681 |

The AVX2 kernels are 7.4× (Fletcher-16), 3.7× (Fletcher-32) and 1.9× (Fletcher-64) faster than the GCC portable loop.
Clang's portable loop is 2.1-2.3× slower than GCC's. With `-march=native` (AVX-VNNI), GCC 16, MB/s:

| Checksum | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Fletcher-16 | 4 136 | 11 095 | 32 186 | 55 934 | 78 507 | 76 821 |
| Fletcher-32 | 5 402 | 9 228 | 20 612 | 40 062 | 50 327 | 51 876 |
| Fletcher-64 | 5 292 | 11 626 | 19 442 | 37 377 | 49 912 | 56 686 |

Short inputs cost a little for the streaming state. At 20 bytes the plain loop of the Wikipedia article runs
Fletcher-32 at 5 990 MB/s and the library at 5 630, 0.94× (GCC 16, `-O2 -march=native`, separate harness). With
Clang 21 for both, the library reaches 0.83× at 20 B and 0.80× at 64 B, because Clang compiles the plain loop faster
(6 370 and 10 970 MB/s). From 128 B the library runs its kernel.

### Cortex-A72

GCC 14.3, `-O2`, AArch64 `-march=armv8-a`, one core, start address aligned. MB/s at 64 B / 256 B / 1500 B / 4 KiB.

| Checksum | AArch64 portable | AArch64 NEON | AArch32 portable | AArch32 NEON |
| --- | --- | --- | --- | --- |
| Fletcher-16 | 710 / 830 / 900 / 910 | 1 100 / 2 600 / 5 280 / 6 560 | 610 / 840 / 970 / 980 | 1 050 / 2 580 / 3 980 / 4 630 |
| Fletcher-32 | 1 290 / 1 750 / 1 790 / 1 850 | 1 180 / 2 860 / 4 740 / 5 260 | 920 / 1 550 / 1 720 / 1 770 | 910 / 2 630 / 4 490 / 5 110 |
| Fletcher-64 | 1 610 / 2 900 / 3 620 / 3 830 | 1 660 / 3 120 / 4 790 / 5 600 | 990 / 1 490 / 1 690 / 1 790 | 940 / 2 500 / 4 580 / 5 500 |

At 4 KiB NEON is 4.7× (AArch32) to 7.2× (AArch64, 64 bytes per iteration) faster for Fletcher-16, 2.8-2.9× for
Fletcher-32, and 1.47× (AArch64) or 3.1× (AArch32) for Fletcher-64, whose 64-bit portable loop is already fast on
AArch64. Below the thresholds both builds run the portable loop; the differences there come from code layout. At 20
bytes AArch64 Fletcher-32 runs 732 MB/s with `-mcpu=cortex-a72`, 0.94× the Wikipedia loop (776), and 732 MB/s with
`-march=armv8-a`, 0.88× (833).

### Cortex-M4

STM32L4A6 at 80 MHz, MB/s at 4 KiB:

| Checksum | Portable | DSP kernel | Kernel build, start address odd |
| --- | --- | --- | --- |
| Fletcher-16 | 15.1 | 29.7 | 29.5 |
| Fletcher-32 | 37.9 | (portable) | 30.7 |
| Fletcher-64 | 44.4 | (portable) | 34.8 |

Fletcher-16 on the STM32L4A6, MB/s:

| Input | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Portable | 5.9 | 10.2 | 12.2 | 13.0 | 13.5 | 14.9 | 15.1 |
| DSP kernel | 5.9 | 12.1 | 17.3 | 20.2 | 22.1 | 28.2 | 29.8 |

The kernel is 2.0× faster at 4 KiB and 1.6× at 256 bytes. On this core the wider sums are cheaper per byte:
Fletcher-64 at 44.4 MB/s is the fastest of the three, and Fletcher-32 and Fletcher-64 are slower from an
odd address, where their word loads are unaligned.

### Cortex-M33

MB/s at 4 KiB:

| Checksum | Portable | DSP kernel | Kernel build, start address odd |
| --- | --- | --- | --- |
| Fletcher-16 | 28.3 | 47.9 | 47.6 |
| Fletcher-32 | 62.4 | (portable) | 55.7 |
| Fletcher-64 | 83.1 | (portable) | 71.5 |

Fletcher-16, MB/s:

| Input | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| Portable | 13.8 | 21.1 | 26.2 | 28.0 | 28.3 |
| DSP kernel | 14.2 | 22.1 | 37.5 | 45.9 | 47.9 |

The kernel is 1.7× faster at 4 KiB and 1.4× at 256 bytes. As on the Cortex-M4 it pays from 64 bytes: at 48 and 63
bytes it would be 4-8 % slower than the portable loop.

### Code size

Cortex-M4, GCC 14.3: the out-of-line loops of the three widths take 2 024 B of code at `-O2` and 1 802 B at `-Os`; the
inline short paths of the header add to each caller.

## fletcher4

User guide: [fletcher4.md](fletcher4.md).

### x86-64

MB/s, `-O2`, one core, each call timed in a loop over the same buffer. Portable is the library with
`CHECKSUM_ACCELERATION=OFF` (portable lanes from 256 B), SSE2 the default x86-64 flags, AVX2 `-march=native`.

| Build | 20 B | 64 B | 128 B | 256 B | 512 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GCC portable | 4 269 | 8 114 | 9 378 | 9 725 | 13 357 | 15 640 | 16 917 | 17 497 |
| GCC SSE2 | 4 402 | 8 233 | 9 756 | 13 545 | 16 092 | 17 811 | 19 318 | 19 703 |
| GCC AVX2 | 4 715 | 9 440 | 12 863 | 18 875 | 21 258 | 26 384 | 29 258 | 30 817 |
| Clang portable | 3 665 | 8 977 | 10 443 | 5 591 | 6 096 | 7 818 | 8 424 | 8 757 |
| Clang SSE2 | 4 293 | 8 886 | 10 437 | 13 512 | 16 259 | 17 679 | 19 246 | 19 416 |
| Clang AVX2 | 3 716 | 9 120 | 11 612 | 16 941 | 22 186 | 26 171 | 28 620 | 29 653 |

Below the thresholds every build runs the word loop; differences of up to 10 % there come from code placement, which
moves with the link order. AVX2 is 1.5× the SSE2 kernel at 4 KiB. With GCC the portable lanes are within 15 % of the
SSE2 kernel from 1500 bytes; with Clang they reach about 45 % of it, which matters only for a build with `CHECKSUM_ACCELERATION=OFF`.

Against the OpenZFS kernels (`fletcher_4_native` of OpenZFS f79c81d with its superscalar4, sse2, ssse3 and avx2 kernels,
all built with GCC `-O2 -march=native`), the AVX2 build runs at 0.93× the fastest at 20 bytes, 1.09× at 256 bytes and
1.0× from 1500 bytes.

### Cortex-A72

GCC 14.3 and Clang 21, `-O2 -mcpu=cortex-a72`, one core. MB/s:

| Build | 20 B | 64 B | 128 B | 256 B | 512 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AArch64 GCC portable | 803 | 1 343 | 1 526 | 2 228 | 2 432 | 2 797 | 3 026 | 2 766 |
| AArch64 GCC NEON | 803 | 1 337 | 1 531 | 2 209 | 2 450 | 2 827 | 3 024 | 2 757 |
| AArch64 Clang portable | 756 | 1 462 | 1 813 | 1 500 | 1 494 | 1 619 | 1 672 | 1 617 |
| AArch64 Clang NEON | 761 | 1 465 | 1 809 | 2 101 | 2 443 | 2 740 | 2 889 | 2 638 |
| AArch32 GCC word loop | 484 | 778 | 839 | 931 | 1 001 | 1 053 | 1 069 | 1 027 |
| AArch32 GCC NEON | 375 | 678 | 769 | 1 126 | 1 504 | 2 046 | 2 384 | 2 408 |
| AArch32 Clang NEON | 325 | 622 | 785 | 1 354 | 1 788 | 2 460 | 2 862 | 2 815 |

On AArch64 with GCC the portable lanes are as fast as NEON; with Clang NEON is 1.4× them at 256 bytes and 1.7× at
4 KiB. On AArch32 NEON is 2.2× the word loop at 4 KiB with GCC. Against OpenZFS (GCC, its superscalar4 and aarch64_neon
kernels) the AArch64 GCC build runs at 0.92× at 20 bytes, 1.18× at 256 bytes and 1.0× from 4 KiB.

### Cortex-M4

Cortex-M cores run the word loop, two words per iteration. nRF52840 at 64 MHz, GCC 14.3 `-O2`, code in flash aligned to
16 bytes, measured with the cycle counter (best of 5 calls), MB/s:

| Start | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| Aligned | 5.5 | 10.9 | 16.8 | 19.8 | 20.2 |
| Odd address | 5.2 | 10.1 | 14.9 | 17.1 | 17.4 |

With one word per iteration the loop ran 18.1 MB/s at 4 KiB (15.8 from an odd address): two words are 1.12× faster
at 4 KiB, 1.08× at 256 bytes.

### Cortex-M33

MB/s:

| Start | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| Aligned | 12.8 | 25.4 | 38.5 | 45.0 | 45.9 |
| Odd address | 12.5 | 24.2 | 35.8 | 41.3 | 42.2 |

Two words per iteration halve the loop overhead around the carry chains of the 64-bit sums: 1.36× at 4 KiB against
one word (33.9 MB/s); four words per iteration were slower than two.

### Code size

Cortex-M4, GCC 14.3: the out-of-line word loop and its callers take 616 B of code at `-O2` and 654 B at `-Os`; the
inline short paths of the header add to each caller.

## xxHash

User guide: [xxhash.md](xxhash.md).

### x86-64

XXH3-64, MB/s, GCC 16. Portable is the library with `CHECKSUM_ACCELERATION=OFF`, SSE2 the
default x86-64 flags, AVX2 a build with AVX2 enabled.

| Build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Portable | 13 269 | 26 667 | 11 060 | 13 165 | 13 932 | 13 809 |
| SSE2 | 12 946 | 26 749 | 16 291 | 21 057 | 22 002 | 21 992 |
| AVX2 | 14 463 | 28 828 | 28 533 | 43 777 | 48 504 | 48 994 |

With Clang 21 at 1 MiB: 20 107 MB/s portable, 32 658 SSE2, 46 907 AVX2. Up to 240 bytes the three builds run the
same code, and 64 bytes are faster than 256 because they need no stripe loop. At 1 MiB AVX2 is 3.5× the
portable loop and SSE2 1.6×. With `-march=native` (AVX2), MB/s at 20 B and 1 MiB: XXH32 6 831 and 8 743, XXH64 8 186 and
18 364, XXH3-128 7 827 and 50 172. With the same flags all four hashes match xxHash v0.8.4 from 64 bytes, except XXH3
at 256 bytes (0.92×), and are faster at 20 bytes.

### Cortex-A72

GCC 14.3, `-O2` (the benchmark presets), one core, MB/s; AArch32 with `-march=armv8-a+crc -mfpu=neon-fp-armv8`:

| Hash, build | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| XXH32, AArch64 | 775 | 1 730 | 2 521 | 2 787 | 2 914 | 2 635 |
| XXH64, AArch64 | 812 | 915 | 1 535 | 1 868 | 1 948 | 1 889 |
| XXH3-64, AArch64 portable | 911 | 1 884 | 2 238 | 3 446 | 3 771 | 3 394 |
| XXH3-64, AArch64 NEON | 910 | 1 885 | 2 643 | 4 280 | 4 704 | 4 113 |
| XXH3-128, AArch64 portable | 875 | 1 866 | 1 864 | 3 275 | 3 699 | 3 415 |
| XXH3-128, AArch64 NEON | 874 | 1 866 | 2 229 | 4 055 | 4 598 | 4 109 |
| XXH32, AArch32 | 633 | 1 460 | 2 352 | 2 767 | 2 904 | 2 580 |
| XXH64, AArch32 | 286 | 367 | 596 | 698 | 731 | 723 |
| XXH3-64, AArch32 portable | 522 | 1 057 | 921 | 1 235 | 1 341 | 1 308 |
| XXH3-64, AArch32 NEON | 522 | 1 057 | 1 544 | 2 494 | 2 807 | 2 652 |
| XXH3-128, AArch32 portable | 442 | 881 | 794 | 1 192 | 1 321 | 1 307 |
| XXH3-128, AArch32 NEON | 441 | 881 | 1 224 | 2 332 | 2 728 | 2 655 |

On AArch64 the NEON kernel is 1.18× the portable loop at 256 bytes and 1.25× at 4 KiB; on AArch32 NEON is 2× faster
from 1500 bytes. XXH64 needs 64-bit multiplications, which AArch32 builds from 32-bit ones; there XXH32 is the fastest
of the four up to 4 KiB. Against xxHash v0.8.4 built the same way, with `-O2` or `-O2 -mcpu=cortex-a72`, XXH3 is 1.0
to 1.15× from 1500 bytes and 0.95 to 1.05× at 256 bytes; XXH32 and XXH64 match it from 256 bytes.

### Cortex-M4

The Cortex-M4 runs the portable code
of every hash. STM32L4A6 at 80 MHz,
MB/s:

| Hash | 20 B | 64 B | 128 B | 192 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| XXH32 | 9.9 | 23.6 | 32.3 | 36.8 | 39.6 | 48.4 | 50.3 |
| XXH64 | 8.4 | 9.3 | 13.4 | 15.8 | 17.3 | 21.6 | 23.6 |
| XXH3-64 | 10.0 | 19.5 | 22.1 | 19.5 | 13.8 | 23.6 | 26.7 |
| XXH3-128 | 8.5 | 16.7 | 19.2 | 11.2 | 10.7 | 21.7 | 25.7 |

On this core XXH32 is the fastest at every size, about twice as fast as the others from 256 bytes. Speed at short
sizes moves by up to 25 % with where the code lands in flash. Against xxHash v0.8.4 built the same way, at 4 KiB XXH64
is faster (23.6 against 20.6 MB/s), XXH3-64 and XXH3-128 are 4-5 % slower (26.7 against 27.7, 25.7 against 27.0), and
XXH32 is 12-14 % slower (50.3 against 57.1; 9.9 against 12.9 MB/s at 20 B).

### Cortex-M33

MB/s:

| Hash | 20 B | 64 B | 256 B | 1500 B | 4 KiB |
| --- | --- | --- | --- | --- | --- |
| XXH32 | 21.7 | 51.2 | 81.9 | 97.2 | 100.8 | 1.27 |
| XXH64 | 15.8 | 16.9 | 30.2 | 38.1 | 40.1 | 3.19 |
| XXH3-64 | 18.6 | 34.3 | 30.3 | 44.4 | 48.5 | 2.64 |
| XXH3-128 | 10.9 | 23.9 | 22.8 | 39.8 | 46.4 | 2.76 |

Clock for clock the Cortex-M33 runs the same code 6-20 % faster than the Cortex-M4. From an odd address every hash is
7-16 % slower at 4 KiB.

### Code size

Cortex-M4, GCC 14.3: the out-of-line loops take 3 648 B of code at `-O2` (XXH32/XXH64 2 672, XXH3 976) and 2 296 B at
`-Os` (1 720, 576); the inline short paths of the header add to each caller.

## MurmurHash3

User guide: [murmur3.md](murmur3.md).

### x86-64

GCC, `-O2 -march=native`: MurmurHash3_x86_32 runs 5 850 MB/s at 20 bytes and 4 499 at 1 MiB, MurmurHash3_x64_128
6 140 and 10 006. Both are at least as fast as SMHasher's MurmurHash3.cpp with the same flags.

### Cortex-A72

GCC 14.3, `-O2`, one core. MB/s:

| Hash, mode | 20 B | 64 B | 256 B | 1500 B | 4 KiB | 1 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| x86_32, AArch64 | 720 | 981 | 1 084 | 1 169 | 1 182 | 1 143 |
| x64_128, AArch64 | 646 | 1 081 | 1 430 | 1 634 | 1 672 | 1 619 |
| x86_32, AArch32 | 618 | 902 | 1 066 | 1 167 | 1 180 | 1 098 |
| x64_128, AArch32 | 278 | 408 | 536 | 595 | 604 | 599 |

On AArch64 MurmurHash3_x86_32 runs 0.95 to 1.0× as fast as SMHasher's MurmurHash3.cpp at every size, with `-O2` or
`-O2 -mcpu=cortex-a72`. In AArch32 it is 0.71× at 20 B, 0.81× at 64 B and 0.97× from 1500 B.

### Cortex-M4

GCC 14.3, `-O2`, code in flash aligned to 16 bytes and data in RAM, measured with the cycle counter (best of 5 calls)
on the nRF52840, converted to the 80 MHz of the STM32L4A6. At 4 KiB MurmurHash3_x86_32 runs 31.7 MB/s (26.5 with one
block per iteration, 1.20× slower) and MurmurHash3_x64_128 18.0 MB/s; at 20 bytes they reach 10.9 and 5.5 MB/s. Without the alignment, code
placement in flash moves these loops by up to 17 %.

### Cortex-M33

At 4 KiB MurmurHash3_x86_32 runs 56.4 MB/s (34.0 with one block per iteration) and MurmurHash3_x64_128 29.7 MB/s; at
20 bytes they reach 19.4 and 10.2 MB/s. Two x64_128 blocks per iteration were 16 % slower: their 64-bit values do not fit the registers.

### Code size

Cortex-M4, GCC 14.3: the block loops of both variants and the one-shot x86_32 function take 900 B of code at `-O2`
and 748 B at `-Os`; the inline short paths of the header add to each caller.
