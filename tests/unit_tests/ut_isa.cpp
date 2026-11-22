// Instruction-set presets: which presets a build has, which presets include others, and what detect_isa() reports.

#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <isa_cpu.hpp>
#include <isa_test_support.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using checksum::isa;
using checksum::isa_available;
using checksum::isa_set;
using checksum::isa_detail::includes;

static_assert(isa_available(isa::generic));

// Each preset includes the presets it extends and generic, nothing of another ladder; the RISC-V presets are independent.
static_assert(includes(isa::x86_avxvnni, isa::x86_pclmul) && includes(isa::x86_avx2, isa::x86_avx2) && !includes(isa::x86_avx2, isa::x86_vpclmul));
static_assert(includes(isa::x86_avx512, isa::x86_vpclmul) && includes(isa::x86_avx512, isa::x86_pclmul) &&
              !includes(isa::x86_avx512, isa::x86_avxvnni) && !includes(isa::x86_avxvnni, isa::x86_avx512) &&
              checksum::isa_detail::line(isa::x86_avx512) == isa::x86_pclmul);
static_assert(isa_set{isa::x86_avx512} == (isa_set{isa::x86_vpclmul} | isa::x86_avx512));
static_assert(includes(isa::x86_sha, isa::x86_pclmul) && !includes(isa::x86_sha, isa::x86_avx2) && !includes(isa::x86_avx512, isa::x86_sha) &&
              includes(isa::aarch64_sha512, isa::aarch64_crc) && !includes(isa::aarch64_sha2, isa::aarch64_sha512));

// An engine runs the loop of its preset or of the nearest preset it extends that the algorithm has one for.
static_assert(checksum::isa_detail::loop_preset(isa::x86_sha, isa_set{isa::x86_avx2}) == isa::x86_pclmul &&
              checksum::isa_detail::loop_preset(isa::x86_avx512, isa_set{isa::x86_avx2}) == isa::x86_avx2 &&
              checksum::isa_detail::loop_preset(isa::aarch64_sha512, isa_set{}) == isa::generic &&
              checksum::isa_detail::loop_preset(isa::riscv_v, isa_set{isa::riscv_v}) == isa::riscv_v);
static_assert(includes(isa::aarch64_pmull, isa::aarch64_crc) && !includes(isa::aarch64_crc, isa::aarch64_pmull));
static_assert(includes(isa::thumb2_crc, isa::thumb2) && includes(isa::thumb2_neon, isa::thumb2_dsp) && !includes(isa::thumb2_dsp, isa::thumb2_neon));
static_assert(!includes(isa::riscv_v, isa::riscv_zbc) && !includes(isa::riscv_zbc, isa::riscv_v));
static_assert(includes(isa::riscv_v, isa::generic) && includes(isa::generic, isa::generic) && !includes(isa::generic, isa::thumb2));
static_assert(!includes(isa::aarch64_pmull, isa::x86_pclmul) && !includes(isa::thumb2, isa::aarch64_crc));

// A build has the presets of its own architecture only.
#if defined(__x86_64__) || defined(_M_X64)
static_assert(isa_available(isa::x86_pclmul) && isa_available(isa::x86_avxvnni) && isa_available(isa::x86_avx512) &&
              !isa_available(isa::aarch64_crc) && !isa_available(isa::thumb2));
#elif defined(__aarch64__) || defined(_M_ARM64)
static_assert(isa_available(isa::aarch64_crc) && isa_available(isa::aarch64_pmull) && !isa_available(isa::x86_avx2) && !isa_available(isa::thumb2));
#elif defined(__riscv) && __riscv_xlen == 64
static_assert(isa_available(isa::riscv_zbc) && !isa_available(isa::x86_avx2)); // riscv_v: not with Clang 18 (presets.hpp)
#elifdef __riscv
static_assert(checksum::isa_detail::built_presets.empty()); // RV32: generic only
#endif

// built_presets is ascending, has no generic, and stays on one line except for RISC-V, whose two presets are independent.
consteval bool built_presets_well_formed() {
  auto const &list = checksum::isa_detail::built_presets;
  for(std::size_t index = 0; index < list.size(); ++index) {
    bool const riscv = list[index] == isa::riscv_zbc || list[index] == isa::riscv_v;
    if(list[index] == isa::generic || (index > 0 && (list[index - 1] >= list[index] ||
                                                     (!riscv && checksum::isa_detail::line(list[index]) != checksum::isa_detail::line(list[0]))))) {
      return false;
    }
  }
  return true;
}

static_assert(built_presets_well_formed());

// A preset appended to the enumeration and to built_presets of isa.hpp must also get a name in isa_cpu.hpp.
TEST(isa_test_support, every_built_preset_has_name) {
  for(isa const preset : checksum::isa_detail::built_presets) {
    EXPECT_FALSE(isa_test::name(preset).empty()) << static_cast<int>(preset);
  }
}

// CHECKSUM_REQUIRED_ISA names only presets that this build has and the CPU runs; the preset tests fail on the others too.
TEST(isa_test_support, required_presets_run) {
  std::optional<isa_set> const required = isa_cpu::required_presets();
  ASSERT_TRUE(required) << "CHECKSUM_REQUIRED_ISA names an unknown preset";
  for(unsigned value = 1; value <= static_cast<unsigned>(isa_cpu::last_preset); ++value) {
    auto const preset = static_cast<isa>(value);
    if(required->contains(preset)) {
      EXPECT_NE(std::ranges::find(checksum::isa_detail::built_presets, preset), checksum::isa_detail::built_presets.end())
          << "this build lacks " << isa_test::name(preset);
      EXPECT_TRUE(isa_test::cpu_runs(preset)) << "the CPU cannot run " << isa_test::name(preset);
    }
  }
}

TEST(detect_isa, contains_only_built_presets_and_their_lower_rungs) {
  isa_set const cpu = checksum::detect_isa();
  EXPECT_TRUE(cpu.contains(isa::generic));
  for(unsigned value = 0; value <= static_cast<unsigned>(isa_cpu::last_preset); ++value) {
    auto const preset = static_cast<isa>(value);
    if(!cpu.contains(preset)) {
      continue;
    }
    EXPECT_TRUE(isa_available(preset)) << isa_test::name(preset);
    for(unsigned lower = 0; lower < value; ++lower) {
      if(includes(preset, static_cast<isa>(lower))) {
        EXPECT_TRUE(cpu.contains(static_cast<isa>(lower))) << isa_test::name(preset) << " without " << isa_test::name(static_cast<isa>(lower));
      }
    }
  }
}

#ifdef __linux__
// The value of the first line of /proc/cpuinfo named key, with a space before and after every word; none if there is none.
std::optional<std::string> cpuinfo_words(std::string_view key) {
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while(std::getline(file, line)) {
    std::size_t const colon = line.find(':');
    if(line.starts_with(key) && colon != std::string::npos && line.find_first_not_of(" \t", key.size()) == colon) {
      return line.substr(colon + 1) + " ";
    }
  }
  return std::nullopt;
}

// The presets that the feature list of /proc/cpuinfo allows, by the kernel's names; none where the list is missing, e.g.
// QEMU user mode showing the host's file.
std::optional<isa_set> cpuinfo_presets() {
  // The last preset of a line whose features and those of every earlier preset are present.
  [[maybe_unused]] auto const ladder = [](std::initializer_list<std::pair<isa, bool>> rungs) {
    isa_set result;
    for(auto const &[preset, present] : rungs) {
      if(!present) {
        break;
      }
      result = preset;
    }
    return result;
  };
#ifdef __x86_64__
  std::optional<std::string> const flags = cpuinfo_words("flags");
  if(!flags) {
    return std::nullopt;
  }
  auto const has = [&](std::string_view flag) { return flags->contains(" " + std::string(flag) + " "); };
  isa_set const vpclmul =
      ladder({{isa::x86_pclmul, has("sse4_2") && has("pclmulqdq")}, {isa::x86_avx2, has("avx2")}, {isa::x86_vpclmul, has("vpclmulqdq")}});
  isa_set result = vpclmul;
  if(vpclmul.contains(isa::x86_pclmul) && has("sha_ni")) {
    result = result | isa::x86_sha;
  }
  if(!vpclmul.contains(isa::x86_vpclmul)) {
    return result;
  }
  if(has("avx_vnni")) {
    result = result | isa::x86_avxvnni;
  }
  if(has("avx512f") && has("avx512bw") && has("avx512vl") && has("avx512_vnni")) {
    result = result | isa::x86_avx512;
  }
  return result;
#elifdef __aarch64__
  std::optional<std::string> const features = cpuinfo_words("Features");
  if(!features) {
    return std::nullopt;
  }
  return ladder({{isa::aarch64_crc, features->contains(" crc32 ")},
                 {isa::aarch64_pmull, features->contains(" pmull ")},
                 {isa::aarch64_sha2, features->contains(" sha2 ")},
                 {isa::aarch64_sha512, features->contains(" sha512 ")}});
#elifdef __arm__
  // Linux builds of the thumb2 presets are ARMv7-A or later, which have Thumb-2 and the DSP extension.
  std::optional<std::string> const features = cpuinfo_words("Features");
  if(!features) {
    return std::nullopt;
  }
  return ladder({{isa::thumb2_dsp, true}, {isa::thumb2_neon, features->contains(" neon ")}, {isa::thumb2_crc, features->contains(" crc32 ")}});
#elifdef __riscv
  // "rv64imafdcv_zicbom_zbc": single-letter extensions, then the others after underscores.
  std::optional<std::string> isa_string = cpuinfo_words("isa");
  if(!isa_string || !isa_string->starts_with(" rv64")) {
    return std::nullopt;
  }
  std::string const letters = isa_string->substr(5, isa_string->find('_') - 5);
  std::ranges::replace(*isa_string, '_', ' ');
  isa_set result;
  if(letters.contains('v')) {
    result = result | isa::riscv_v;
  }
  if(isa_string->contains(" zbc ")) {
    result = result | isa::riscv_zbc;
  }
  return result;
#else
  return std::nullopt;
#endif
}

// detect_isa() reports exactly the built presets that the CPU runs: those that CHECKSUM_EXPECTED_ISA names (for runs in an
// emulator, whose /proc/cpuinfo may be the host's) or else those of /proc/cpuinfo.
TEST(detect_isa, matches_the_cpu) {
  std::optional<std::string> const names = isa_cpu::environment("CHECKSUM_EXPECTED_ISA");
  std::optional<isa_set> const expected = names ? isa_cpu::named_presets(*names) : cpuinfo_presets();
  ASSERT_TRUE(expected || !names) << "CHECKSUM_EXPECTED_ISA names an unknown preset: " << *names;
  if(!expected) {
    GTEST_SKIP() << "neither CHECKSUM_EXPECTED_ISA nor a feature list in /proc/cpuinfo";
  }
  isa_set const detected = checksum::detect_isa();
  for(isa const preset : checksum::isa_detail::built_presets) {
    // Kernels before 6.8 and older QEMU have no Zbc key in riscv_hwprobe, so detection may miss what /proc/cpuinfo lists.
    if(!names && preset == isa::riscv_zbc && !detected.contains(preset)) {
      continue;
    }
    EXPECT_EQ(detected.contains(preset), expected->contains(preset)) << isa_test::name(preset);
  }
}
#endif

} // namespace
