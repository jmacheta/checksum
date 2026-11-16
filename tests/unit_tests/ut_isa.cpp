// detect_isa(): the best variant compiled into this build and supported by the running CPU, consistent with
// __builtin_cpu_supports.

#include <checksum/isa.hpp>
#include <gtest/gtest.h>

namespace {

using namespace checksum;

// Repeats the decision detect_isa() must make: a kernel compiled for the variant, and this CPU running it.
isa_id expected_isa() {
#if defined(CHECKSUM_INTERNET_X86_AVX2) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
  if(__builtin_cpu_supports("avx2")) {
    return isa_id::x86_avx2;
  }
#endif
  return isa_id::none;
}

TEST(isa, detect_matches_builtin_cpu_supports) { EXPECT_EQ(detect_isa(), expected_isa()); }

TEST(isa, detect_never_names_an_uncompiled_variant) {
#ifndef CHECKSUM_INTERNET_X86_AVX2
  // The build compiled no kernel, whatever the CPU supports.
  EXPECT_EQ(detect_isa(), isa_id::none);
#endif
  EXPECT_TRUE(detect_isa() == isa_id::none || detect_isa() == isa_id::x86_avx2);
}

} // namespace
