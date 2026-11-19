# fletcher4 user guide

`checksum` computes fletcher4, the checksum of the ZFS file system, at compile time or at run time. Everything is in
one header, `checksum/fletcher4.hpp`. The Fletcher-16, Fletcher-32 and Fletcher-64 checksums, which use a modulus and
two sums, have their own guide: [fletcher.md](fletcher.md).

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, Clang ≥ 18, or MSVC (x64, ARM64). It has
no tables, allocates no memory and has no global state.

## 1. Quick start

```cpp
#include <checksum/fletcher4.hpp>
using namespace std::literals;

// One call.
checksum::fletcher4_value value = checksum::fletcher4_compute(std::span(block));

// Incremental: a message may be split anywhere.
checksum::fletcher4_state state;
state = checksum::fletcher4_update(state, first_part);
state = checksum::fletcher4_update(state, second_part);
checksum::fletcher4_value checksum_words = checksum::fletcher4_finalize(state);

// Compile time.
static_assert(checksum::fletcher4_compute("abcdefgh"sv) ==
              checksum::fletcher4_value{0xCCCAC8C6, 0x1312E2B27, 0x195918D88, 0x1F9F4EFE9});
```

`examples/fletcher4` has complete programs: the checksum of a constant block computed at compile time, the checksum of
a file read in chunks, and verifying a 4 KiB block against its stored checksum as ZFS does.

## 2. Definition

fletcher4 reads the message as 32-bit little-endian words and keeps four 64-bit sums, all starting at 0. For each word
`sum1` adds the word, `sum2` adds `sum1`, `sum3` adds `sum2` and `sum4` adds `sum3`, all modulo 2^64. The checksum is
`{sum1, sum2, sum3, sum4}`, the order of the four words of the ZFS `zio_cksum_t`.

Words are little-endian on every host. On a little-endian host the result equals `fletcher_4_native` of OpenZFS, on a
big-endian host `fletcher_4_byteswap`. The test values were computed with `fletcher_4_scalar_native` of OpenZFS on a
little-endian host.

ZFS checksums whole words only. Here a message may end inside a word: the last word is padded with zero bytes, so
`"abc"` gives the same checksum as `"abc\0"`. During streaming the bytes of an unfinished word are already added to
`sum1`; `fletcher4_finalize` completes the padded word by adding to `sum2`, `sum3` and `sum4`.

## 3. API

`Isa` is an instruction-set preset from `checksum/isa.hpp`; the default is `isa::generic`.

| Name | What it does |
| --- | --- |
| `fletcher4_state` | `sum1` to `sum4` and `word_offset`, the bytes of an unfinished word already in `sum1`. A plain value: copy it, compare it, store it. The default is the empty message. |
| `fletcher4_update<Isa>(state, data)` | Folds `data` into `state` and returns the new state. `Isa` selects the run-time loop; see section 5. |
| `fletcher4_finalize(state)` | The checksum, an unfinished word padded with zero bytes. |
| `fletcher4_compute<Isa>(data)` | `fletcher4_finalize(fletcher4_update<Isa>(fletcher4_state{}, data))`. |
| `fletcher4_value` | The checksum: `std::array<std::uint64_t, 4>` holding `sum1` to `sum4`. |

All functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. Contiguous ranges are passed on as one
span and other ranges in 64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never
summed: pass text as `std::string_view`.

Every state value is valid, so nothing has preconditions: `word_offset` is taken modulo 4.

Every function that takes `Isa` also has a value overload with an `isa_set` first, e.g. `fletcher4_compute(cpu, data)` with
`cpu = checksum::detect_isa()`: it runs the best preset of this build in the set; see
[Instruction-set presets](../README.md#instruction-set-presets).

## 4. Behavior

- **Splits:** a message may be split anywhere, including inside a word; the result is the same as for one call.
- **Zero:** the empty message and an all-zero message of any length give `{0, 0, 0, 0}`, so leading zero words do not
  change the checksum.
- **Wraparound:** the sums wrap modulo 2^64, as in ZFS; there is no length limit.
- **Byte order:** words are read little-endian on every target. The result is four numbers; ZFS decides how they are
  stored.

## 5. CPU acceleration

The word loop adds one word per step, a dependency chain of four additions. Longer inputs run four interleaved lanes:
lane j sums the words j, j + 4, j + 8, ... as a message of its own, and the four lane results are combined with
weights that follow from the definition. The sums wrap modulo 2^64 in both forms, so the result is exact. The lanes
run in vector registers where a kernel exists, else in portable code:

| Preset | Lanes |
| --- | --- |
| `isa::x86_avx2`, `x86_vpclmul`, `x86_avxvnni` | One 256-bit vector per sum, from 128 B |
| `isa::generic`, `x86_pclmul` on x86-64 (SSE2 baseline) | Two 128-bit vectors per sum, from 192 B |
| every preset of little-endian AArch64, `isa::generic` included (NEON is always there) | NEON, two 128-bit vectors per sum, from 192 B |
| `isa::thumb2_neon`, `thumb2_crc` on little-endian 32-bit Arm | NEON, from 192 B |
| other 64-bit targets: `isa::generic`, `isa::riscv_zbc`, `isa::riscv_v`, big-endian AArch64 | Portable lanes, from 256 B |
| other 32-bit targets: `isa::generic`, `isa::thumb2`, `isa::thumb2_dsp`, big-endian 32-bit Arm | Word loop only; two words per iteration on 32-bit Arm with any preset but `thumb2_neon` and `thumb2_crc` |

`fletcher4_compute<checksum::isa::x86_avx2>(data)` runs the AVX2 lanes; how presets work and what to check before
calling one is in [Instruction-set presets](../README.md#instruction-set-presets). MSVC uses the same kernels as GCC and
Clang, NEON on ARM64 included. On 32-bit targets the 64-bit sums of four portable lanes do not fit the registers and ran
3× slower than the word loop, so those targets never use them. The measurements behind each choice are in
[design/acceleration.md](design/acceleration.md#fletcher4).

## 6. Performance guidelines

Measured figures are in [performance.md](performance.md#fletcher4).

- **Lanes start at a minimum size** of one `fletcher4_update` call (section 5): 128 B with AVX2, 192 B with SSE2 or
  NEON, 256 B for the portable lanes. ZFS blocks are larger, so they always run the lanes; with AVX2 a 4 KiB block
  hashes at about 29 000 MB/s, on par with OpenZFS.
- **The portable lanes run only without a lane kernel** (RISC-V, big-endian AArch64). Measured where a kernel exists,
  Clang's portable lanes reached about 45 % of the SSE2 kernel on x86-64 and 60 % of NEON on AArch64; GCC's are close to
  the kernels.
- **On 32-bit targets without a lane kernel**, including Cortex-M, the word loop runs about 20 MB/s at 64 MHz on a
  Cortex-M4. Aligned buffers are 16 % faster than odd start addresses.

## 7. Limitations

- **Weak on short messages and zero words:** leading zero words do not change the checksum, and the sums of a short
  message stay small. Use a CRC where errors must be detected reliably.
- **Little-endian words only:** the equivalent of `fletcher_4_native` on a big-endian host needs its words swapped
  first.
- **No continuation from a stored checksum:** unlike Adler-32 and Fletcher, continuing needs the state, since a
  checksum does not record an unfinished word.
