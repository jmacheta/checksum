#ifndef CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP
#define CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP

// Test helpers for instruction-set presets: the list of presets this build has, and a typed-test fixture over them that
// skips the presets the CPU cannot run.

#include <checksum/isa.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace isa_test {

template <checksum::isa Preset> using preset = std::integral_constant<checksum::isa, Preset>;

// The presets of the list Rest that this build has, after Kept, as testing::Types; generic alone if there are none.
template <class Kept, checksum::isa... Rest> struct available;

template <class... Kept> struct available<std::tuple<Kept...>> {
  using type = testing::Types<Kept...>;
};

template <> struct available<std::tuple<>> {
  using type = testing::Types<preset<checksum::isa::generic>>;
};

template <class... Kept, checksum::isa First, checksum::isa... Rest>
struct available<std::tuple<Kept...>, First, Rest...>
    : available<std::conditional_t<checksum::isa_available(First), std::tuple<Kept..., preset<First>>, std::tuple<Kept...>>, Rest...> {};

// Every preset but generic that this build has.
using presets = available<std::tuple<>, checksum::isa::x86_pclmul, checksum::isa::x86_avx2, checksum::isa::x86_vpclmul, checksum::isa::x86_avxvnni,
                          checksum::isa::aarch64_crc, checksum::isa::aarch64_pmull, checksum::isa::thumb2, checksum::isa::thumb2_dsp,
                          checksum::isa::thumb2_neon, checksum::isa::thumb2_crc, checksum::isa::riscv_zbc, checksum::isa::riscv_v>::type;

inline constexpr std::array<std::string_view, 13> names{"generic",     "x86_pclmul",    "x86_avx2", "x86_vpclmul", "x86_avxvnni",
                                                        "aarch64_crc", "aarch64_pmull", "thumb2",   "thumb2_dsp",  "thumb2_neon",
                                                        "thumb2_crc",  "riscv_zbc",     "riscv_v"};

// Names the typed tests after their preset.
struct preset_name {
  template <class Preset> static std::string GetName(int /*index*/) { return std::string(names.at(static_cast<std::size_t>(Preset::value))); }
};

// Whether the running CPU executes the code of preset. x86-64 asks the CPU; the other architectures run their tests in
// QEMU with -cpu max, which has every extension.
inline bool cpu_runs(checksum::isa preset) {
#if defined(__x86_64__)
  bool const pclmul = __builtin_cpu_supports("sse4.2") && __builtin_cpu_supports("pclmul");
  bool const avx2 = pclmul && __builtin_cpu_supports("avx2");
  bool const vpclmul = avx2 && __builtin_cpu_supports("vpclmulqdq");
  switch(preset) {
  case checksum::isa::x86_pclmul:
    return pclmul;
  case checksum::isa::x86_avx2:
    return avx2;
  case checksum::isa::x86_vpclmul:
    return vpclmul;
  case checksum::isa::x86_avxvnni:
    return vpclmul && __builtin_cpu_supports("avxvnni");
  default:
    return true;
  }
#else
  static_cast<void>(preset);
  return true;
#endif
}

// Typed-test fixture over presets: skips a preset the CPU cannot run.
template <class Preset> class preset_test : public testing::Test {
protected:
  void SetUp() override {
    if(!cpu_runs(Preset::value)) {
      GTEST_SKIP() << "the CPU cannot run it";
    }
  }
};

} // namespace isa_test

#endif // CHECKSUM_TESTS_ISA_TEST_SUPPORT_HPP
