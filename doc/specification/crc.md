# CRC specification

Status: draft 9 · Applies to: `checksum` v0.1 · Keywords MUST / SHOULD / MAY as in RFC 2119.

## 1. Scope

`checksum` is a cross-platform C++23 library computing checksums. v0.1 implements the
CRC family: any width 1..64, Rocksoft/RevEng parameter model, byte- and bit-granular
messages. Other checksum algorithms are out of scope for this specification; the
`checksum::crc` namespace leaves room for them as siblings.

Non-goals for v0.1: thread safety, frame verification and residue checks (built by users from `compute`/`update`), widths above 64, CRC combination (`crc32_combine`),
CPU-specific acceleration (design deferred, §9.7), writing CRCs into buffers,
"indirect"/augmented `init` conventions (only the RevEng "direct" convention is used),
converting or formatting polynomials into other notations. A C API is never provided.

## 2. General requirements

| ID | Requirement |
| --- | --- |
| GEN-1 | ISO C++23 only, no compiler extensions in the core. Supported: GCC ≥ 13, Clang ≥ 17, with libstdc++ or libc++, on hosted implementations including bare-metal toolchains with a hosted C++ library (e.g. arm-none-eabi + newlib). MSVC best effort. |
| GEN-2 | The core is fully portable: no intrinsics, inline assembly, OS calls or endianness assumptions. Platform-specific code is only allowed in user strategies (§9.2) and future acceleration (§9.7). |
| GEN-3 | No exceptions, no RTTI, no dynamic allocation, no I/O; builds with `-fno-exceptions -fno-rtti`. |
| GEN-4 | Every public function that can be `constexpr` is `constexpr` and `noexcept`; results are identical at compile time and run time. |
| GEN-5 | **Strong contract:** every object of a library type is valid. Types with invariants have private data, no public constructor that could break them, and are created by factories returning `std::optional<T>` (no library error enum/category). |
| GEN-6 | Preconditions on arguments that are not objects of library types (e.g. a bit count) are checked by `CHECKSUM_ASSERT(cond)`. During constant evaluation a violation is always a compile error. At run time the check is active unless `NDEBUG` is defined, and always when `CHECKSUM_HARDENED` is defined; a failed check calls `[[noreturn]] void checksum::assertion_failed(std::source_location) noexcept`. Its default definition is alone in `src/assert_handler.cpp`; a user replaces it by defining the same function in their own translation unit (the archive member is then not linked). `CHECKSUM_ASSERT` appears only in header-inline entry points, never in the compiled loops of `src/`; `NDEBUG` and `CHECKSUM_HARDENED` must be the same in all translation units of a program. With checks disabled, a violation produces an unspecified but valid result (arguments are clamped or masked as documented per function), never UB. |
| GEN-7 | No undefined behavior; the whole test suite passes under the ASan+UBSan preset. |
| GEN-8 | Not thread-safe: objects are unsynchronized values. The library has no mutable global state, so distinct objects may be used concurrently, and a `const` engine (§9.3) may be shared by threads. |
| GEN-9 | Naming per `.clangd`: `lower_case` functions, variables, namespaces, scoped enumerators; `CamelCase` types, concepts and template parameters. Small value types (≤ 40 B) are passed and compared by value; engines by `const&`. |
| GEN-10 | Bounded template bloat: no *functions* are instantiated per CRC parameter set (`engine<P, S>` instantiates data only). Out-of-line run-time loops of built-in strategies are compiled once in `src/crc/*.cpp`: at most 4 register types × 2 reflections per strategy. Thin inline adapters are instantiated per argument range type. |
| GEN-11 | Data crosses every non-template boundary as `std::span<std::byte const>`; templates accepting other byte ranges are thin adapters. |

## 3. Headers

| Header | Content |
| --- | --- |
| `checksum/assert.hpp` | `CHECKSUM_ASSERT`, `checksum::assertion_failed`, `CHECKSUM_NO_UNIQUE_ADDRESS`. |
| `checksum/crc.hpp` | Umbrella: includes all `checksum/crc/*.hpp`. |
| `checksum/crc/polynomial.hpp` | `crc::Polynomial`, notation functions, literals. |
| `checksum/crc/polynomials.hpp` | `crc::polynomials::*` well-known generator polynomials. |
| `checksum/crc/parameters.hpp` | `crc::Parameters`, `crc::ParametersSpec`, `crc::CrcRegister`, `crc::RegisterFor<W>`. |
| `checksum/crc/table.hpp` | `crc::StrategyFor`, `crc::Strategy`, built-in table strategies. |
| `checksum/crc/bytes.hpp` | `crc::ByteLike`, `crc::ByteRange`. |
| `checksum/crc/engine.hpp` | `crc::Engine<Reg, S>`, `crc::engine<P, S>`. |
| `checksum/crc/accumulator.hpp` | `crc::Accumulator<Reg, S>`. |
| `checksum/crc/catalog.hpp` | `crc::catalog::*` RevEng parameter sets, `entries`, `find`. |

Private headers: `src/include_private/checksum_private/`, namespace `checksum::detail`.

## 4. Byte input (`checksum/crc/bytes.hpp`)

```cpp
namespace checksum::crc {
template <class T> concept ByteLike = std::same_as<std::remove_cv_t<T>, std::byte> ||
  std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, unsigned char> ||
  std::same_as<std::remove_cv_t<T>, signed char> || std::same_as<std::remove_cv_t<T>, char8_t>;

template <class R> concept ByteRange = std::ranges::input_range<R> &&
  ByteLike<std::ranges::range_value_t<R>> &&
  !(std::is_array_v<std::remove_cvref_t<R>> &&                       // reject string literals
    (std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, char> ||
     std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, char8_t>));
}
```

| ID | Requirement |
| --- | --- |
| IN-1 | Every function taking data has a `std::span<std::byte const>` overload; `ByteRange` overloads are thin adapters. They forward contiguous ranges at run time as one span. Non-contiguous ranges at run time, and every range other than a contiguous `std::byte` range during constant evaluation, are copied in chunks of 64 bytes into a local `std::byte` array, each chunk passed to the span overload. |
| IN-2 | Arrays of `char`/`char8_t` (string literals) are rejected by `ByteRange`, since their `'\0'` would silently be included; text is passed as `std::string_view`, and other `char` buffers (e.g. `char rx[64]`) are wrapped in `std::span`. Arrays of `std::byte`, `unsigned char` (`std::uint8_t`) and `signed char` are accepted. |

## 5. `crc::Polynomial`

A non-template value type holding a valid generator polynomial: its **normal**
representation without the implicit x^width term, and its width.

```cpp
namespace checksum::crc {
class Polynomial {
public:
  static constexpr std::optional<Polynomial> create(std::uint64_t normal, unsigned width) noexcept;
  constexpr std::uint64_t normal() const noexcept;   // bit i = coefficient of x^i, i < width
  constexpr unsigned width() const noexcept;          // degree, 1..64
  friend constexpr bool operator==(Polynomial, Polynomial) noexcept = default;
private:
  constexpr Polynomial(std::uint64_t normal, std::uint8_t width) noexcept;
  std::uint64_t normal_;
  std::uint8_t width_;
};
```

| ID | Requirement |
| --- | --- |
| POLY-1 | `create` returns `std::nullopt` iff `width ∉ [1, 64]` or `normal ≥ 2^width`. It is the only way to obtain a `Polynomial` besides copying, so every `Polynomial` is valid. There is no default constructor. |
| POLY-2 | The class has no templates and no knowledge of notations; all notations are free functions built on `create` (§5.1), so users add notations without modifying the class. |
| POLY-3 | Trivially copyable, `sizeof(Polynomial) == 16` on common ABIs. A polynomial without the x^0 term is valid. |

### 5.1 Notations (free functions, `namespace checksum::crc`)

The public API only *creates* polynomials; it does not convert them back into other
notations (the internal inverse conversions used by tests live in `checksum::detail`).

| Function | Notation (CRC-16/CCITT example) |
| --- | --- |
| `from_normal(v, w)` ≡ `Polynomial::create` | x^w implicit: `0x1021`, w = 16 |
| `from_reversed(v, w)` | bit-reflected normal: `0x8408` |
| `from_reciprocal(v, w)` | reciprocal polynomial, x^w implicit: `0x0811` |
| `from_koopman(v)` | x^w explicit, x^0 implicit, width = `bit_width(v)`: `0x8810` |
| `from_full(v)` | all terms, width = `bit_width(v) − 1`: `0x11021` |
| `parse_algebraic(text)` | `"x^16 + x^12 + x^5 + 1"` |

Signatures: `constexpr std::optional<Polynomial> from_X(std::uint64_t value[, unsigned width]) noexcept;`,
`constexpr std::optional<Polynomial> parse_algebraic(std::string_view text) noexcept;`.
`from_normal`, `from_reversed` and `from_reciprocal` return `nullopt` iff `w ∉ [1, 64]` or `v ≥ 2^w`. `from_reciprocal` produces polynomials with the x^0 term only (its leading coefficient is
implicit). `from_koopman(0)` and `from_full(v)` with `v < 2` return `nullopt`.

```cpp
namespace literals {
consteval Polynomial operator""_poly(char const* text, std::size_t size) noexcept;   // parse_algebraic
consteval Polynomial operator""_koopman(unsigned long long value) noexcept;          // from_koopman
}
```

| ID | Requirement |
| --- | --- |
| NOT-1 | Algebraic grammar over ASCII. Whitespace (space, tab) is allowed before and after every token, including around `^` and at both ends; `x` and `X` are equivalent. `poly := term ('+' term)*`; `term := 'x' ['^' exponent] \| '1'`; `exponent := digit+` in 0..64, leading zeros allowed (`x^016` ≡ `x^16`). `x` ≡ `x^1`, `1` ≡ `x^0`. Terms in any order. The highest exponent is the width and must be ≥ 1 (so `"1"` and `"x^0"` are rejected). Rejected (`nullopt`): empty or whitespace-only input, duplicate terms (`x^0` and `1` are duplicates), empty term (`x^8++1`, leading or trailing `+`), exponent > 64 or more than 3 digits, signs, any other character. |
| NOT-2 | Literals are `consteval`; invalid input calls the non-`constexpr` function `detail::invalid_polynomial_literal()`, so compilation fails with that name in the diagnostic. |
| NOT-3 | Each of `from_reversed(0x8408, 16)`, `from_reciprocal(0x0811, 16)`, `from_koopman(0x8810)`, `from_full(0x11021)` and `parse_algebraic("x^16 + x^12 + x^5 + 1")` compares equal to `from_normal(0x1021, 16)`. |

### 5.2 Well-known polynomials (`checksum/crc/polynomials.hpp`)

`namespace checksum::crc::polynomials` holds `inline constexpr Polynomial` constants for
generator polynomials in common use, named by width and established name, e.g.
`crc8_ccitt` (0x07), `crc8_dallas` (0x31), `crc16_ibm` (0x8005), `crc16_ccitt` (0x1021),
`crc24_openpgp` (0x864CFB), `crc32_ieee` (0x04C11DB7), `crc32c_castagnoli` (0x1EDC6F41),
`crc32k_koopman` (0x741B8CD7), `crc64_ecma` (0x42F0E1EBA9EA3693), `crc64_iso` (0x1B).

| ID | Requirement |
| --- | --- |
| WKP-1 | Every polynomial used by a catalog entry (§8) exists here; catalog entries are defined using these constants. A polynomial without an established name is named `poly<w>_<lower-case normal hex>`, e.g. `poly16_8bb7`. |
| WKP-2 | Each constant's comment cites RevEng (normal form) and, where listed, Koopman's CRC Zoo (Koopman form). |

## 6. `crc::Parameters`

The Rocksoft model. `check` is catalog data (§8).

```cpp
struct ParametersSpec {          // plain input aggregate, unchecked
  Polynomial poly;               // required: Polynomial has no default constructor
  std::uint64_t init = 0;
  bool refin = false;
  bool refout = false;
  std::uint64_t xorout = 0;
};

class Parameters {               // validated, sizeof <= 40
public:
  static constexpr std::optional<Parameters> create(ParametersSpec spec) noexcept;
  constexpr Polynomial poly() const noexcept;
  constexpr unsigned width() const noexcept;
  constexpr std::uint64_t init() const noexcept;
  constexpr bool refin() const noexcept;
  constexpr bool refout() const noexcept;
  constexpr std::uint64_t xorout() const noexcept;
  constexpr ParametersSpec spec() const noexcept;
  friend constexpr bool operator==(Parameters, Parameters) noexcept = default;
};

template <class R> concept CrcRegister = /* one of uint8_t, uint16_t, uint32_t, uint64_t */;
template <unsigned W> requires (W >= 1 && W <= 64) using RegisterFor = /* smallest CrcRegister with >= W bits */;
```

| ID | Requirement |
| --- | --- |
| PAR-1 | `create` returns `std::nullopt` iff `init` or `xorout` has bits at or above `width`. `Parameters::create(p.spec()) == p`. |
| PAR-2 | `init` and `xorout` are in natural (unreflected) order, exactly as listed by RevEng. |
| PAR-3 | User CRCs: `inline constexpr auto my_crc = *crc::Parameters::create({.poly = "x^16 + x^15 + x^2 + 1"_poly, .init = 0xFFFF, .refin = true, .refout = true});`. Dereferencing an empty optional in a constant expression fails to compile. |

## 7. Bit-granular messages

A message is a sequence of bits. Byte-aligned data is a `std::span<std::byte const>`;
a trailing partial byte is passed separately:

- `update(state, data)`: all bits of `data`;
- `update_bits(state, std::byte tail, unsigned count)`: `count` ∈ [0, 8] bits of `tail` (precondition; clamped to 8 when unchecked); `count == 0` returns `state`;
- `update(state, data, std::size_t bit_length)`: the first `bit_length` bits of `data` (precondition `bit_length ≤ 8·data.size()`; clamped when unchecked) ≡ `update` of the first `bit_length / 8` bytes, then `update_bits` of the next byte with `bit_length % 8`.

| ID | Requirement |
| --- | --- |
| BIT-1 | The *bit order* of the message is LSB first if `refin`, MSB first otherwise. A tail uses bits `0..count−1` (LSB first) or `7..8−count` (MSB first); other bits of `tail` are ignored. |
| BIT-2 | `update_bits(s, b, 8) == update(s, std::span{&b, 1})`; splitting a bit sequence at any bit position gives the same result as one call. |

## 8. Catalog (`checksum/crc/catalog.hpp`)

| ID | Requirement |
| --- | --- |
| CAT-1 | `namespace checksum::crc::catalog` contains every entry with width ≤ 64 of the RevEng "Catalogue of parametrised CRC algorithms" (edition pinned in the header; CRC-82/DARC is excluded) as `inline constexpr Parameters`, named from the RevEng name in lower case with `/` and `-` mapped to `_` (`crc32_iso_hdlc`, `crc16_ibm_3740`). Common aliases are `inline constexpr Parameters const&` references to the same object: `crc32` → `crc32_iso_hdlc`, `crc32c` → `crc32_iscsi`, `crc16_ccitt_false` → `crc16_ibm_3740`, `crc16_x25` → `crc16_ibm_sdlc`, `crc64` → `crc64_xz`. `engine<alias>` and `engine<canonical>` are the same object. |
| CAT-2 | `struct Entry { std::string_view name; Parameters const* parameters; std::uint64_t check; };` (`parameters` never null) and `inline constexpr std::array<Entry, N> entries` with every canonical entry once; `name` is the RevEng name, `check` = CRC of ASCII `"123456789"`. |
| CAT-3 | `constexpr std::optional<Entry> find(std::string_view name) noexcept`: ASCII case-insensitive; accepts RevEng names (`"CRC-32/ISO-HDLC"`), the C++ names (`"crc32_iso_hdlc"`) and the aliases. |
| CAT-4 | A program that names only some entries contains only those (no code, no other data). Using `entries` or `find` at run time odr-uses all entries (≈ 40 B each). |

## 9. Computation

### 9.1 Register model

The state of a CRC computation is a register `r` of type `Reg` (`CrcRegister`, at least `width` bits):

- if `refin`: `r` is bit-reflected over `width`, initial value `reflect(init, width)`;
- otherwise: `r` is in normal order, low-aligned, initial value `init`;
- bits at or above `width` are zero after every operation;
- every operation taking a state has the precondition `s < 2^width` (`CHECKSUM_ASSERT`; masked to `width` bits when unchecked).

`finalize(r) = (refin != refout ? reflect(r, width) : r) ^ xorout`.

### 9.2 Strategies (`checksum/crc/table.hpp`)

A strategy is a stateless type defining how whole bytes are folded into the register
and which precomputed data that needs. Built-in strategies are the lookup-table variants:

| Type | Data per engine | Notes |
| --- | --- | --- |
| `crc::table_none` | none | bitwise; smallest flash/RAM |
| `crc::table_partial` | 16 × `Reg` | nibble table |
| `crc::table_full` (default) | 256 × `Reg` | byte table |
| `crc::table_sliced` | 8 × 256 × `Reg` | slicing-by-8; fastest portable variant |

```cpp
template <class S, class Reg> concept StrategyFor = CrcRegister<Reg> &&
  std::semiregular<typename S::template table_type<Reg>> &&
  requires(Parameters p, typename S::template table_type<Reg> const& t, Reg r, std::span<std::byte const> d) {
    { S::template make_table<Reg>(p) } -> std::same_as<std::optional<typename S::template table_type<Reg>>>;
    { S::template update<Reg>(t, p, r, d) } -> std::same_as<Reg>;
  };
template <class S> concept Strategy = StrategyFor<S, std::uint8_t> && StrategyFor<S, std::uint16_t> &&
                                      StrategyFor<S, std::uint32_t> && StrategyFor<S, std::uint64_t>;
```

| ID | Requirement |
| --- | --- |
| STR-1 | `update` folds whole bytes and obeys §9.1. Bit tails (§7) are handled by the engine bitwise, so strategies never see them. |
| STR-2 | `make_table` returns `nullopt` for parameters the strategy cannot handle (e.g. a peripheral supporting only one polynomial); built-in strategies accept every valid `Parameters` whose width fits `Reg`. |
| STR-3 | Built-in strategies model `Strategy`; their `make_table` and `update` are `constexpr`. A user strategy needs to model only `StrategyFor<S, Reg>` for the registers it supports and MAY be non-`constexpr` (e.g. an MCU CRC peripheral); it is then used through `Engine::create` at run time only. |
| STR-4 | Users add strategies without modifying the library, e.g. `crc::Engine<std::uint32_t, stm32_crc_unit>::create(crc::catalog::crc32_mpeg_2)`. |
| STR-5 | All built-in strategies give bit-identical results for every catalog entry, every length 0..1024 and every buffer alignment. |
| STR-6 | Built-in run-time loops are non-template functions in `src/crc/`, one per (register type × reflection) per strategy, taking the table by pointer; the header selects between them and the `constexpr` loop with `if consteval`. `table_sliced` handles the last `size % 8` bytes with its first slice (≡ `table_full`); non-reflected CRCs narrower than `Reg` (including widths < 8) run top-aligned internally: the state is shifted once on entry and once on exit of each `update` call, the public state stays low-aligned. |

### 9.3 `crc::Engine<Reg, S>`

```cpp
template <CrcRegister Reg, StrategyFor<Reg> S = table_full>
class Engine {
public:
  using state_type = Reg;
  using result_type = Reg;
  using strategy_type = S;

  // nullopt iff p.width() > std::numeric_limits<Reg>::digits or S::make_table<Reg>(p) is nullopt
  static constexpr std::optional<Engine> create(Parameters p) noexcept;

  constexpr Parameters parameters() const noexcept;
  constexpr state_type initial() const noexcept;
  constexpr state_type update(state_type s, std::span<std::byte const> data) const noexcept;
  template <ByteRange R> constexpr state_type update(state_type s, R&& data) const noexcept;
  constexpr state_type update(state_type s, std::span<std::byte const> data, std::size_t bit_length) const noexcept;
  constexpr state_type update_bits(state_type s, std::byte tail, unsigned count) const noexcept;
  constexpr result_type finalize(state_type s) const noexcept;

  constexpr result_type compute(std::span<std::byte const> data) const noexcept;
  template <ByteRange R> constexpr result_type compute(R&& data) const noexcept;
  constexpr result_type compute(std::span<std::byte const> data, std::size_t bit_length) const noexcept;
private:
  Parameters params_;
  CHECKSUM_NO_UNIQUE_ADDRESS typename S::template table_type<Reg> table_;  // built by create()
};

template <Parameters const& P, Strategy S = table_full>
inline constexpr Engine<RegisterFor<P.width()>, S> engine = *Engine<RegisterFor<P.width()>, S>::create(P);
```

| ID | Requirement |
| --- | --- |
| ENG-1 | `Engine` is immutable after `create` (no non-`const` members besides assignment). The evolving CRC is the separate `state_type` value, so one engine serves any number of concurrent computations. |
| ENG-2 | One type serves compile-time and run-time parameters: `crc::engine<crc::catalog::crc32>` is a `constexpr` object whose table is in read-only data; `Engine<std::uint32_t>::create(runtime_params)` builds the table at run time. `engine<P, S>` requires `S` to model `Strategy` with `constexpr` `make_table` and `update`; other strategies are used through `Engine::create`. |
| ENG-3 | `Reg` wider than needed is allowed (`Engine<std::uint64_t>` for a CRC-16) and gives the same results; `RegisterFor` picks the smallest. Results always fit in `width` bits. |
| ENG-4 | `compute(d) ≡ finalize(update(initial(), d))`, likewise for the bit-length overload. Chunking invariance: for every byte split `d = d1 ++ d2`, `finalize(update(update(initial(), d1), d2)) == compute(d)`; bit splits as in BIT-2. |
| ENG-5 | Empty tables take no space (`CHECKSUM_NO_UNIQUE_ADDRESS` = `[[no_unique_address]]`, `[[msvc::no_unique_address]]` on MSVC). Documented sizes (LP64): `table_none` == `sizeof(Parameters)` (≤ 40 B), `Engine<uint32_t, table_full>` ≈ 1 KiB, `Engine<uint64_t, table_full>` ≈ 2 KiB, `Engine<uint64_t, table_sliced>` ≈ 16 KiB — large engines belong in static storage, not on an MCU stack. |
| ENG-6 | `engine<P, S>` is ill-formed when instantiated with a `P` for which `S::make_table` returns `nullopt` (only possible for user strategies). |

### 9.4 `crc::Accumulator` (`checksum/crc/accumulator.hpp`)

```cpp
template <CrcRegister Reg, StrategyFor<Reg> S = table_full>
class Accumulator {
public:
  constexpr explicit Accumulator(Engine<Reg, S> const& e) noexcept;   // stores &e and e.initial()
  Accumulator(Engine<Reg, S> const&&) = delete;                        // no dangling temporaries
  constexpr Accumulator& update(std::span<std::byte const> data) noexcept;
  template <ByteRange R> constexpr Accumulator& update(R&& data) noexcept;
  constexpr Accumulator& update(std::span<std::byte const> data, std::size_t bit_length) noexcept;
  constexpr Accumulator& update_bits(std::byte tail, unsigned count) noexcept;
  constexpr Reg value() const noexcept;               // finalize(state), non-destructive
  constexpr Reg state() const noexcept;
  constexpr void reset() noexcept;
};
```

| ID | Requirement |
| --- | --- |
| ACC-1 | Trivially copyable; holds only `Engine<Reg, S> const*` and a `Reg` state; `Reg` and `S` are deduced from the engine (`crc::Accumulator acc{engine}`). |
| ACC-2 | `value()` ≡ `engine.finalize(state())` and does not change the state; `reset()` sets the state to `engine.initial()`. |
| ACC-3 | Each `update`/`update_bits` ≡ the corresponding `Engine` call on `state()`; the preconditions of §7 apply. |

### 9.5 Usage

```cpp
#include <checksum/crc.hpp>
using namespace std::literals;
using namespace checksum::crc::literals;
namespace crc = checksum::crc;

static_assert(crc::engine<crc::catalog::crc32>.compute("123456789"sv) == 0xCBF43926);

// own CRC with a nibble table (small flash)
inline constexpr auto my_crc = *crc::Parameters::create({.poly = 0x8810_koopman, .init = 0xFFFF});
crc::Accumulator acc{crc::engine<my_crc, crc::table_partial>};
acc.update(header).update(payload);
std::uint16_t v = acc.value();

// CAN: CRC-15 over an 83-bit message
std::array<std::byte, 11> bits = /* ... */;
constexpr auto const& can = crc::engine<crc::catalog::crc15_can>;
std::uint16_t c = can.compute(bits, 83);

// parameters known only at run time
auto e = crc::parse_algebraic(user_text)
           .and_then([](crc::Polynomial p) { return crc::Parameters::create({.poly = p}); })
           .and_then(crc::Engine<std::uint64_t>::create);
```

### 9.6 Extension summary

| User wants | Mechanism | Library change |
| --- | --- | --- |
| own CRC | `Parameters::create` (+ `_poly`, `_koopman`) | none |
| own polynomial notation | free function returning `std::optional<Polynomial>` via `Polynomial::create` | none |
| own table / hardware backend | type modeling `StrategyFor<S, Reg>` | none |

### 9.7 CPU acceleration (design deferred)

Not part of v0.1; the mechanism will be specified separately. The v0.1 design keeps it
possible without configuration macros for acceleration in public headers (the only public macros are those of `checksum/assert.hpp`): built-in run-time loops are out-of-line
functions in `src/crc/` (STR-6), so faster implementations can be added in source files
selected by CMake without changing headers or results.

## 10. Performance

| ID | Requirement |
| --- | --- |
| PERF-1 | Benchmarks (Google Benchmark, `tests/benchmarks`, option `CHECKSUM_BENCHMARKS`, default OFF) cover every built-in strategy, widths 8/16/32/64, reflected and not, inputs 16 B / 256 B / 4 KiB / 1 MiB. |
| PERF-2 | x86-64, `-O2`, 4 KiB input: `table_full` ≥ 4× `table_none` throughput; `table_sliced` ≥ 2× `table_full` for widths 32 and 64. |
| PERF-3 | `table_sliced` for CRC-32/ISO-HDLC reaches ≥ 80 % of zlib's portable (non-SIMD) `crc32` throughput at 4 KiB and 1 MiB (zlib is a benchmark-only dependency). |
| PERF-4 | 16 B inputs through `engine<P, table_full>.compute` take ≤ 1.3× the time of a fully inlined `table_full` loop in the same benchmark. |

## 11. Testing

| ID | Requirement |
| --- | --- |
| TST-1 | GoogleTest, `tests/unit_tests/ut_<area>.cpp`, run by `ctest` in every preset including `native-asan`. |
| TST-2 | Catalog: for every entry × built-in strategy × {`engine<P, S>`, run-time `Engine::create`, `Engine<uint64_t>`}: `check` verified at run time (typed tests) and at compile time: each check evaluates `engine<*entries[I].parameters, S>.compute(...)` for one `I` (combined by a fold over `std::make_index_sequence<N>`), so each table is built in its own variable-template initializer; no `-fconstexpr-steps`/`-fconstexpr-ops-limit` flags are used, which proves every table fits the default GCC and Clang limits. |
| TST-3 | Reference vectors: `tests/vectors/generate.py` produces `tests/vectors/crc_vectors.inc` (committed) with CRCs of fixed-seed random messages, byte lengths 0..300 and bit lengths 1..300, for every catalog entry. Bit-length vectors come from a plain bitwise Python implementation of the RevEng model in the script; that implementation is cross-checked against the `reveng` tool for all byte-length vectors. |
| TST-4 | Polynomials: every `polynomials::` constant against RevEng normal form and CRC Zoo Koopman form; NOT-3 over all of them, and round trips through the internal inverse conversions over random polynomials of every width 1..64 (with and without x^0). |
| TST-5 | Equivalence: lengths 0..1024, start offsets 0..7, every built-in strategy; chunked vs one-shot (every byte split ≤ 64 B, every bit split ≤ 64 bits, `bit_length` 0 and multiples of 8); `Reg` wider than needed, with bits above `width` checked to stay zero. |
| TST-6 | Edge parameters: width 1 and width 64 with `init` and `xorout` all ones; widths 1..7 with `refin = false` and `true` on every strategy; refin ≠ refout (CRC-12/UMTS plus a synthetic model whose expected vectors come from a pinned `reveng` version, stored in `crc_vectors.inc`). |
| TST-7 | Parser: one test per grammar rule and per rejection listed in NOT-1. A user-defined test-double strategy (supporting one `Reg` and one polynomial, `make_table` rejecting others); `Accumulator` equals `compute` for every split of TST-5. |
| TST-8 | Negative compile tests (CTest `WILL_FAIL` targets): invalid `_poly` literal, invalid `Parameters` in `constexpr`, `engine<P, S>` where `S` is a second test double modeling `Strategy` with `constexpr` members whose `make_table` rejects `P`, string literal passed to `update`, `Accumulator` from a temporary engine. |
