# xxHash user guide

`checksum` computes the non-cryptographic hashes XXH32, XXH64, XXH3-64 and XXH3-128 with a seed, at compile time or at
run time. The values are those of the reference implementation (`xxhash.h` 0.8.4) on every target. XXH32 and XXH64 are
in `checksum/xxhash.hpp`, XXH3-64 and XXH3-128 in `checksum/xxh3.hpp`.

The requirements are the same as for the rest of the library: C++23, GCC ≥ 14, or Clang ≥ 18. Nothing allocates
memory and there is no global state; XXH3 keeps its 192-byte default secret in read-only data.

## 1. Quick start

```cpp
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>
using namespace std::literals;

// One call, seed 0 or another seed.
std::uint32_t small = checksum::xxh32_compute(key);
std::uint64_t hash = checksum::xxh3_64_compute(key, 1234);
checksum::hash128 wide = checksum::xxh3_128_compute(key);

// Incremental: a message may be split anywhere.
checksum::xxh3_64_state state{.seed = 1234};
state = checksum::xxh3_update(state, first_part);
state = checksum::xxh3_update(state, second_part);
std::uint64_t value = checksum::xxh3_finalize(state);

// Compile time.
static_assert(checksum::xxh32_compute("abc"sv) == 0x32D153FF);
static_assert(checksum::xxh64_compute("abc"sv) == 0x44BC2CF5AD770999);
static_assert(checksum::xxh3_64_compute("abc"sv) == 0x78AF5F94892F3950);
static_assert(checksum::xxh3_128_compute("abc"sv) ==
              checksum::hash128{.low = 0x78AF5F94892F3950, .high = 0x06B05AB6733A6185});
```

`examples/xxh3` has complete programs: asset IDs hashed at compile time, the XXH3-64 hash of a file read in chunks,
deduplication of blocks by their XXH3-128 hash, and a hash table with seeded XXH3-64.

## 2. API

Both families have the same four names with their own prefix: a state, `update`, `finalize` and `compute`. All
functions are `constexpr` and `noexcept`. `data` is a `std::span<std::byte const>` or any range of `std::byte`,
`char`, `unsigned char`, `signed char` or `char8_t`, as for the other algorithms. `compute` hashes contiguous ranges
directly, without the state's buffer; `update` passes contiguous ranges on as one span and folds other ranges in
64-byte chunks. Arrays of `char` are rejected, so the `'\0'` of a string literal is never hashed: pass text as
`std::string_view`.

Every state value is valid, so nothing has preconditions. A state is a plain value: copy it, store it, and hash one
prefix of a message several ways.

### 2.1 XXH32 and XXH64

`Width` is 32 for XXH32 and 64 for XXH64.

| Name | What it does |
| --- | --- |
| `xxhash_state<Width>` | The four lane accumulators, the number of bytes folded (`length`), the `seed` and the bytes of an unfinished stripe (`buffer`, 16 or 32 bytes). The default is the empty message with seed 0; `xxhash_state<Width>{.seed = seed}` starts one with another seed. |
| `xxhash_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxhash_finalize(state)` | The hash of the message folded into `state`; the state is unchanged. |
| `xxhash_compute<Width>(data, seed = 0)` | `xxhash_finalize(xxhash_update(xxhash_state<Width>{.seed = seed}, data))`. |
| `xxh32_state`, `xxh64_state` | `xxhash_state<32>` and `xxhash_state<64>`. |
| `xxh32_compute(data, seed = 0)`, `xxh64_compute(data, seed = 0)` | `xxhash_compute<32>` and `xxhash_compute<64>`. |

`xxhash_state<Width>::value_type` is the type of the hash, the seed and the lanes: `std::uint32_t` for XXH32 and
`std::uint64_t` for XXH64. The lanes are set from the seed when the first whole stripe is folded; before that they are
ignored.

`examples/xxhash` has complete programs: message IDs hashed at compile time, the XXH64 of a file read in chunks, and
a cache keyed by the hash of its input.

### 2.2 XXH3-64 and XXH3-128

`Width` is 64 for XXH3-64 and 128 for XXH3-128.

| Name | What it does |
| --- | --- |
| `xxh3_state<Width>` | The eight accumulators, the number of bytes passed (`length`), the `seed` and a 256-byte `buffer`: 336 bytes. The default is the empty message with seed 0; `xxh3_state<Width>{.seed = seed}` starts one with another seed. |
| `xxh3_update(state, data)` | Folds `data` into `state` and returns the new state. |
| `xxh3_finalize(state)` | The hash of the message folded into `state`; the state is unchanged. |
| `xxh3_compute<Width>(data, seed = 0)` | `xxh3_finalize(xxh3_update(xxh3_state<Width>{.seed = seed}, data))`. |
| `hash128` | The XXH3-128 hash: `low` and `high`, its lower and upper 64 bits, as `XXH128_hash_t` of the reference. Compares with `==`. MurmurHash3_x64_128 returns the same type. |
| `xxh3_64_state`, `xxh3_128_state` | `xxh3_state<64>` and `xxh3_state<128>`. |
| `xxh3_64_compute(data, seed = 0)`, `xxh3_128_compute(data, seed = 0)` | `xxh3_compute<64>` and `xxh3_compute<128>`. `xxh3_update` and `xxh3_finalize` take the width from the state. |

`xxh3_state<Width>::value_type` is `std::uint64_t` for Width 64 and `hash128` for Width 128. The seed is a
`std::uint64_t` for both. The hashes always use the default secret; a custom secret cannot be passed.

The state keeps the last 1 to 256 bytes passed in its buffer and folds the rest into the accumulators, so it can
hash the end of the message as the one-shot function does. Messages up to 240 bytes are hashed from the buffer alone.

## 3. Behavior

- **Splits:** a message may be split anywhere; the result is the same as for one call.
- **Seed:** different seeds give unrelated hashes. XXH3 with seed 0 hashes with the default secret; another seed
  derives a 192-byte secret from it on the stack, for each call that folds stripes (a one-shot hash of more than
  240 bytes, an `xxh3_update` that folds its buffer, or `xxh3_finalize` of more than 240 bytes).
- **Byte order:** input words are read little-endian on every target, so a big-endian target gives the same hashes.
  The result is a number or a pair of numbers. The canonical form of the reference writes a hash most significant byte
  first, and for XXH3-128 `high` before `low`.
- **Long messages:** the length is a 64-bit count; XXH32 mixes it in modulo 2^32, as the reference does.
- **Streaming cost:** `xxh3_update` takes the state by value and returns it, which copies its 336 bytes twice per
  call. Folding works in place, so the cost is per call, not per byte: in 64-byte pieces XXH3 streams at about
  2 600 MB/s against 10 000 MB/s for an in-place update (x86-64, SSE2 kernel), about 4× slower; from 4 KiB pieces the copy is
  negligible. Hash short pieces into a buffer of your own, or call `xxh3_compute` once when the whole message is in
  memory. With a seed other than 0, each update that folds its buffer builds the secret again: about 25 % slower than
  seed 0 in 256-byte pieces, 6 % in 4 KiB pieces, on x86-64. The `byte_range` overload of `xxh3_update` folds its
  chunks into one state without copying it.

## 4. CPU acceleration

### 4.1 XXH32 and XXH64: no kernel

XXH32 and XXH64 have no kernel, on any target. Each of the four lanes is a serial chain of a multiplication and a
rotation per word; the scalar loop already keeps four of them in flight. A NEON prototype of XXH32 ran 1 685 MB/s on a
Cortex-A72 at 4 KiB against 2 333 for the scalar loop, and Clang vectorizing the lanes on its own lost 38 % on
x86-64. XXH64 needs 64-bit multiplications per lane, which SSE2, AVX2 and NEON do not have. The stripe loop is
compiled once per width in `src/xxhash/stripe_loop.cpp`, out of line, so that compilers keep the lanes scalar; the
one-shot hash of a message of at least one stripe runs a function there that keeps them in local variables, for XXH32
on every target and for XXH64 on 64-bit targets.

### 4.2 XXH3

Inputs up to 240 bytes run straight-line code with no loop and no kernel. Longer inputs fold 64-byte stripes into eight
64-bit accumulators, and a kernel holds them in vector registers:

| Target and flags | Kernel |
| --- | --- |
| x86-64 with AVX2 (`-mavx2`, or a `-march` that includes it) | Two 256-bit vectors, every stripe. |
| x86-64 without AVX2 (SSE2 is always there) | Four 128-bit vectors, every stripe. |
| AArch64, little-endian | Four lanes in NEON and four in scalar registers, every stripe. |
| 32-bit Arm with NEON, little-endian | All eight lanes in NEON, every stripe. |
| RISC-V RV64 with the V extension, VLEN ≥ 128 | One register group of eight 64-bit lanes, every stripe. Tested in QEMU, not measured on hardware. |
| everything else (Cortex-M, 32-bit Arm without NEON, big-endian, RISC-V without V), or `CHECKSUM_ACCELERATION=OFF` | Portable loop. |

The kernel is selected at compile time from the compiler flags; there is no run-time CPU detection. Constant
evaluation always runs the portable code with the same result. On AArch64 four NEON lanes with four scalar ones
beat six and two, and all eight in NEON. The measurements behind each choice are in
[design/acceleration.md](design/acceleration.md#xxh3).

## 5. Performance guidelines

Measured figures are in [performance.md](performance.md#xxhash).

- **Pick the hash for the CPU.** On 64-bit CPUs XXH3 is the fastest, about 48 000 MB/s with AVX2 on x86-64. On
  32-bit cores (Cortex-M, AArch32) XXH32 is: on a Cortex-M4 it is about twice as fast as the others from 256 bytes,
  and XXH64 is slow there, since it needs 64-bit multiplications.
- **XXH3 kernels start above 240 bytes.** Shorter inputs run straight-line code, the same in every build. On x86-64,
  AVX2 doubles the speed of the default SSE2 build for long inputs.
- **Hash whole messages in one call** when they are in memory. Streaming XXH3 in small pieces is slow (section 3):
  64-byte pieces are about 4× slower than one call, and from 4 KiB pieces the cost is negligible. A seed other than 0
  adds about 25 % in 256-byte pieces and 6 % in 4 KiB pieces.

## 6. Limitations

- **Not cryptographic:** none of these hashes resists inputs crafted to collide, even with a secret seed. Do not use
  them for message authentication or to protect hash tables from hostile keys.
- **Not an error check:** unlike a CRC, they guarantee the detection of no class of errors, burst errors included.
- **Default secret only:** XXH3 takes a seed but no custom secret, so the reference's `XXH3_64bits_withSecret` and
  related variants are not provided.
