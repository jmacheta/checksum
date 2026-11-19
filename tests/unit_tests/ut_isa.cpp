// Instruction-set presets: which presets a build has, and which presets include others.

#include <checksum/isa.hpp>

#include <gtest/gtest.h>
#include <isa_test_support.hpp>

#include <cstddef>

namespace {

using checksum::isa;
using checksum::isa_available;
using checksum::isa_detail::includes;

static_assert(isa_available(isa::generic));

// Each line includes its earlier presets and generic, nothing of another line; the RISC-V presets are independent.
static_assert(includes(isa::x86_avxvnni, isa::x86_pclmul) && includes(isa::x86_avx2, isa::x86_avx2) && !includes(isa::x86_avx2, isa::x86_vpclmul));
static_assert(includes(isa::aarch64_pmull, isa::aarch64_crc) && !includes(isa::aarch64_crc, isa::aarch64_pmull));
static_assert(includes(isa::thumb2_crc, isa::thumb2) && includes(isa::thumb2_neon, isa::thumb2_dsp) && !includes(isa::thumb2_dsp, isa::thumb2_neon));
static_assert(!includes(isa::riscv_v, isa::riscv_zbc) && !includes(isa::riscv_zbc, isa::riscv_v));
static_assert(includes(isa::riscv_v, isa::generic) && includes(isa::generic, isa::generic) && !includes(isa::generic, isa::thumb2));
static_assert(!includes(isa::aarch64_pmull, isa::x86_pclmul) && !includes(isa::thumb2, isa::aarch64_crc));

// A build has the presets of its own architecture only.
#if defined(__x86_64__) || defined(_M_X64)
static_assert(isa_available(isa::x86_pclmul) && isa_available(isa::x86_avxvnni) && !isa_available(isa::aarch64_crc) && !isa_available(isa::thumb2));
#elif defined(__aarch64__) || defined(_M_ARM64)
static_assert(isa_available(isa::aarch64_crc) && isa_available(isa::aarch64_pmull) && !isa_available(isa::x86_avx2) && !isa_available(isa::thumb2));
#elif defined(__riscv)
static_assert(isa_available(isa::riscv_zbc) && isa_available(isa::riscv_v) && !isa_available(isa::x86_avx2));
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
TEST(isa_test_support, EveryBuiltPresetHasName) {
  for(isa preset : checksum::isa_detail::built_presets) {
    EXPECT_FALSE(isa_test::name(preset).empty()) << static_cast<int>(preset);
  }
}

} // namespace
