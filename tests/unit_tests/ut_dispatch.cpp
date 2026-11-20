// The run-time layer: isa_set, detect_isa(), the dispatch engines of every algorithm must run a
// variant of this build that the set allows and give the results of isa::generic.

#include <checksum/adler32.hpp>
#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>
#include <checksum/engine.hpp>
#include <checksum/fletcher.hpp>
#include <checksum/fletcher4.hpp>
#include <checksum/internet.hpp>
#include <checksum/isa.hpp>
#include <checksum/murmur3.hpp>
#include <checksum/xxh3.hpp>
#include <checksum/xxhash.hpp>

#include <gtest/gtest.h>
#include <isa_test_support.hpp>
#include <test_data.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;

// A preset is the set of its line up to it; generic is in every set.
static_assert(isa_set{}.contains(isa::generic) && !isa_set{}.contains(isa::x86_pclmul));
static_assert(isa_set{isa::x86_vpclmul}.contains(isa::x86_pclmul) && isa_set{isa::x86_vpclmul}.contains(isa::generic));
static_assert(!isa_set{isa::x86_avx2}.contains(isa::x86_vpclmul) && !isa_set{isa::x86_avx2}.contains(isa::aarch64_crc));
static_assert(isa_set{isa::thumb2_crc}.contains(isa::thumb2_dsp) && !isa_set{isa::riscv_v}.contains(isa::riscv_zbc));
static_assert((isa_set{isa::riscv_zbc} | isa::riscv_v).contains(isa::riscv_zbc) && (isa_set{isa::riscv_zbc} | isa::riscv_v).contains(isa::riscv_v));

// select() takes the latest preset of this build in both the CPU's set and the algorithm's own-loop set, else generic. An
// own-loop set holds the presets with target options plus the presets without them (RISC-V) that the algorithm has a kernel
// for: riscv_zbc for CRC, riscv_v for Internet and XXH3, none for the others; xxHash, MurmurHash3 and the CRC strategies
// without preset loops hold generic alone.
constexpr isa_set riscv_both = isa_set{isa::riscv_zbc} | isa::riscv_v;
constexpr isa_set target_options = isa_detail::built_with_target_options();
static_assert(isa_detail::select(isa_set{}, target_options) == isa::generic);
static_assert(isa_detail::select(isa::x86_vpclmul, target_options) == (isa_available(isa::x86_vpclmul) ? isa::x86_vpclmul : isa::generic));
static_assert(isa_detail::select(isa::aarch64_pmull, target_options) == (isa_available(isa::aarch64_pmull) ? isa::aarch64_pmull : isa::generic));
static_assert(isa_detail::select(riscv_both, target_options | isa::riscv_v) == (isa_available(isa::riscv_v) ? isa::riscv_v : isa::generic));
static_assert(isa_detail::select(riscv_both, target_options | isa::riscv_zbc) == (isa_available(isa::riscv_zbc) ? isa::riscv_zbc : isa::generic));
static_assert(isa_detail::select(riscv_both, target_options) == isa::generic);
static_assert(isa_detail::select(isa::riscv_v, target_options | isa::riscv_zbc) == isa::generic &&
              isa_detail::select(isa::riscv_zbc, target_options | isa::riscv_v) == isa::generic);
static_assert(isa_detail::select(isa::x86_avxvnni, isa_set{}) == isa::generic && isa_detail::select(isa::thumb2_crc, isa_set{}) == isa::generic &&
              isa_detail::select(riscv_both, isa_set{}) == isa::generic);
static_assert(internet_detail::own_loops == (target_options | isa::riscv_v) && xxh3_detail::own_loops == (target_options | isa::riscv_v) &&
              adler32_detail::own_loops == target_options && fletcher_detail::own_loops == target_options &&
              fletcher4_detail::own_loops == target_options && xxhash_detail::own_loops == isa_set{} && murmur3_detail::own_loops == isa_set{});
static_assert(crc_detail::own_loops<crc_lut_sliced, std::uint32_t> == (target_options | isa::riscv_zbc) &&
              crc_detail::own_loops<crc_lut_braided, std::uint64_t> == (target_options | isa::riscv_zbc) &&
              crc_detail::own_loops<crc_lut_byte, std::uint32_t> == isa_set{});

// The dispatch engines report the preset whose loop runs.
static_assert(internet_dispatch_engine(riscv_both).preset() == isa_detail::select(riscv_both, isa::riscv_v));
static_assert(xxh3_dispatch_engine<64>(0, riscv_both).preset() == isa_detail::select(riscv_both, isa::riscv_v));
static_assert(internet_dispatch_engine(isa::riscv_zbc).preset() == isa::generic &&
              xxh3_dispatch_engine<128>(0, isa::riscv_zbc).preset() == isa::generic);
static_assert(adler32_dispatch_engine(riscv_both).preset() == isa::generic && fletcher_dispatch_engine<32>(riscv_both).preset() == isa::generic &&
              fletcher4_dispatch_engine(riscv_both).preset() == isa::generic && xxhash_dispatch_engine<64>(0, riscv_both).preset() == isa::generic &&
              murmur3_dispatch_engine<32>(0, riscv_both).preset() == isa::generic);
static_assert(xxhash_dispatch_engine<64>(0, isa::x86_avx2).preset() == isa::generic &&
              murmur3_dispatch_engine<128>(0, isa::aarch64_pmull).preset() == isa::generic &&
              crc_dispatch_engine<std::uint32_t, crc_lut_byte>::create(crc32c, isa::x86_vpclmul)->preset() == isa::generic);
static_assert(crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(crc32c, isa::riscv_v)->preset() == isa::generic);

// Constant evaluation runs the portable code for any set.
static_assert(internet_dispatch_engine(isa::x86_avx2).compute("123456789"sv) == internet_compute("123456789"sv));
static_assert(fletcher_dispatch_engine<32>(isa::thumb2_neon).compute("abcde"sv) == fletcher_compute<32>("abcde"sv));
static_assert(crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(crc32_iso_hdlc, isa::aarch64_pmull)->compute("123456789"sv) == 0xCBF43926U);
static_assert(xxh3_dispatch_engine<64>(7, isa::x86_vpclmul).compute("abc"sv) == xxh3_compute<64>("abc"sv, 7));

// Static and dispatch engines model the same concepts; the hashes cannot resume.
static_assert(resumable_engine<internet_dispatch_engine> && resumable_engine<adler32_dispatch_engine> &&
              resumable_engine<fletcher_dispatch_engine<32>> && resumable_engine<fletcher4_dispatch_engine> &&
              resumable_engine<crc_dispatch_engine<std::uint16_t, crc_lut_sliced>>);
static_assert(engine<xxhash_dispatch_engine<32>> && engine<xxh3_dispatch_engine<128>> && engine<murmur3_dispatch_engine<32>>);
static_assert(!resumable_engine<xxhash_dispatch_engine<64>> && !resumable_engine<xxh3_dispatch_engine<64>> &&
              !resumable_engine<murmur3_dispatch_engine<128>>);

// Strategies without kernels choose generic, whatever the set: their loop is the same for every preset.
static_assert(crc_dispatch_engine<std::uint32_t, crc_lut_none>::create(crc32_iso_hdlc, isa::x86_avxvnni)->preset() == isa::generic);
static_assert(crc_dispatch_engine<std::uint32_t, crc_lut_nibble>::create(crc32_iso_hdlc, isa::aarch64_pmull)->preset() == isa::generic);
static_assert(crc_dispatch_engine<std::uint32_t, crc_lut_byte>::create(crc32_iso_hdlc, isa::riscv_zbc)->preset() == isa::generic);

constexpr std::array<std::size_t, 9> sizes{0, 1, 3, 63, 64, 255, 1500, 4097, 70'000};

template <crc_parameters Parameters, class Strategy> void check_crc(isa_set cpu, std::vector<std::byte> const &buffer) {
  auto const &reference = crc_engine_for<Parameters, crc_lut_byte>;
  using register_type = std::remove_cvref_t<decltype(reference)>::state_type;
  auto const engine = crc_dispatch_engine<register_type, Strategy>::create(Parameters, cpu);
  static_assert(std::same_as<typename decltype(engine)::value_type::value_type, register_type>);
  ASSERT_TRUE(engine.has_value());
  EXPECT_TRUE(cpu.contains(engine->preset()) && isa_available(engine->preset()));
  for(std::size_t const size : sizes) {
    for(std::size_t const offset : {0UZ, 1UZ}) {
      auto const data = at(buffer, offset, size);
      SCOPED_TRACE(testing::Message() << "size " << size << ", offset " << offset);
      EXPECT_EQ(engine->compute(data), reference.compute(data));
      EXPECT_EQ(engine->finalize(engine->update(engine->update(engine->initial(), data.first(size / 3)), data.subspan(size / 3))),
                reference.compute(data));
      EXPECT_EQ(engine->resume(reference.compute(data)), reference.resume(reference.compute(data)));
      if(size != 0) {
        EXPECT_EQ(engine->compute(data, (8 * size) - 3), reference.compute(data, (8 * size) - 3));
      }
    }
  }
  std::list<std::byte> const list(buffer.begin(), buffer.begin() + 300);
  EXPECT_EQ(engine->compute(list), reference.compute(list));
}

void check_crcs(isa_set cpu, std::vector<std::byte> const &buffer) {
  check_crc<crc32_iso_hdlc, crc_lut_sliced>(cpu, buffer);
  check_crc<crc32c, crc_lut_sliced>(cpu, buffer);
  check_crc<crc32_bzip2, crc_lut_braided>(cpu, buffer);
  check_crc<crc16_xmodem, crc_lut_sliced>(cpu, buffer);
  check_crc<crc64_xz, crc_lut_braided>(cpu, buffer);
  check_crc<crc8_smbus, crc_lut_byte>(cpu, buffer);
}

// Every member of a dispatch engine against the static engine of isa::generic, from a state other than the initial one.
// own_loops is the algorithm's own-loop set (isa_detail::select()).
template <class Generic, class Dispatch>
void check_engine(Generic const &generic, Dispatch const &dispatch, isa_set own_loops, isa_set cpu, std::vector<std::byte> const &buffer) {
  static_assert(std::same_as<typename Generic::state_type, typename Dispatch::state_type>);
  static_assert(std::same_as<typename Generic::value_type, typename Dispatch::value_type>);
  EXPECT_TRUE(cpu.contains(dispatch.preset()) && isa_available(dispatch.preset()));
  EXPECT_EQ(dispatch.preset(), isa_detail::select(cpu, own_loops));
  EXPECT_EQ(dispatch.initial(), generic.initial());
  auto const start = generic.update(generic.initial(), at(buffer, 2, 37));
  for(std::size_t const size : sizes) {
    for(std::size_t const offset : {0UZ, 1UZ}) {
      auto const data = at(buffer, offset, size);
      SCOPED_TRACE(testing::Message() << "size " << size << ", offset " << offset);
      EXPECT_EQ(dispatch.compute(data), generic.compute(data));
      EXPECT_EQ(dispatch.update(start, data), generic.update(start, data));
      EXPECT_EQ(dispatch.finalize(dispatch.update(start, data)), generic.finalize(generic.update(start, data)));
      if constexpr(resumable_engine<Generic>) {
        EXPECT_EQ(dispatch.resume(generic.compute(data)), generic.resume(generic.compute(data)));
      }
    }
  }
  std::list<std::byte> const list(buffer.begin(), buffer.begin() + 300);
  EXPECT_EQ(dispatch.compute(list), generic.compute(list));
  EXPECT_EQ(dispatch.update(start, list), generic.update(start, list));
}

// Every dispatch engine made for cpu against the generic template.
void check_engines(isa_set cpu, std::vector<std::byte> const &buffer) {
  check_engine(internet_engine{}, internet_dispatch_engine(cpu), target_options | isa::riscv_v, cpu, buffer);
  check_engine(adler32_engine{}, adler32_dispatch_engine(cpu), target_options, cpu, buffer);
  check_engine(fletcher_engine<16>{}, fletcher_dispatch_engine<16>(cpu), target_options, cpu, buffer);
  check_engine(fletcher_engine<32>{}, fletcher_dispatch_engine<32>(cpu), target_options, cpu, buffer);
  check_engine(fletcher_engine<64>{}, fletcher_dispatch_engine<64>(cpu), target_options, cpu, buffer);
  check_engine(fletcher4_engine{}, fletcher4_dispatch_engine(cpu), target_options, cpu, buffer);
  check_engine(xxh3_engine<64>{5}, xxh3_dispatch_engine<64>(5, cpu), target_options | isa::riscv_v, cpu, buffer);
  check_engine(xxh3_engine<128>{}, xxh3_dispatch_engine<128>(0, cpu), target_options | isa::riscv_v, cpu, buffer);
  check_engine(xxhash_engine<32>{3}, xxhash_dispatch_engine<32>(3, cpu), isa_set{}, cpu, buffer);
  check_engine(xxhash_engine<64>{}, xxhash_dispatch_engine<64>(0, cpu), isa_set{}, cpu, buffer);
  check_engine(murmur3_engine<32>{9}, murmur3_dispatch_engine<32>(9, cpu), isa_set{}, cpu, buffer);
  check_engine(murmur3_engine<128>{2}, murmur3_dispatch_engine<128>(2, cpu), isa_set{}, cpu, buffer);
}

// Default-constructed engines detect the CPU themselves.
TEST(dispatch, default_engines_choose_the_detected_preset) {
  isa const best = isa_detail::select(detect_isa(), target_options);
  isa const best_rvv = isa_detail::select(detect_isa(), target_options | isa::riscv_v);
  EXPECT_EQ(internet_dispatch_engine{}.preset(), best_rvv);
  EXPECT_EQ(adler32_dispatch_engine{}.preset(), best);
  EXPECT_EQ(fletcher_dispatch_engine<16>{}.preset(), best);
  EXPECT_EQ(fletcher_dispatch_engine<32>{}.preset(), best);
  EXPECT_EQ(fletcher_dispatch_engine<64>{}.preset(), best);
  EXPECT_EQ(fletcher4_dispatch_engine{}.preset(), best);
  EXPECT_EQ(xxh3_dispatch_engine<64>{}.preset(), best_rvv);
  EXPECT_EQ(xxh3_dispatch_engine<128>{}.preset(), best_rvv);
  EXPECT_EQ(xxhash_dispatch_engine<32>{}.preset(), isa::generic);
  EXPECT_EQ(xxhash_dispatch_engine<64>{}.preset(), isa::generic);
  EXPECT_EQ(murmur3_dispatch_engine<32>{}.preset(), isa::generic);
  EXPECT_EQ(murmur3_dispatch_engine<128>{}.preset(), isa::generic);
  auto const crc = crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(crc32_iso_hdlc);
  ASSERT_TRUE(crc.has_value());
  EXPECT_EQ(crc->preset(), isa_detail::select(detect_isa(), target_options | isa::riscv_zbc));
  EXPECT_EQ((crc_dispatch_engine<std::uint32_t, crc_lut_braided>::create(crc32_iso_hdlc)->preset()), crc->preset());
  EXPECT_EQ((crc_dispatch_engine<std::uint32_t, crc_lut_byte>::create(crc32_iso_hdlc)->preset()), isa::generic);
  EXPECT_FALSE((crc_dispatch_engine<std::uint8_t, crc_lut_sliced>::create(crc32_iso_hdlc).has_value()));
  auto const data = random_bytes(5000, 14);
  EXPECT_EQ(crc->compute(data), (crc_engine_for<crc32_iso_hdlc, crc_lut_byte>.compute(data)));
}

TEST(dispatch, detected_set_matches_generic) {
  auto const buffer = random_bytes(70'008, 11);
  check_engines(detect_isa(), buffer);
  check_crcs(detect_isa(), buffer);
}

TEST(dispatch, presets_of_other_architectures_run_generic) {
  auto const buffer = random_bytes(70'008, 12);
  for(isa const preset : {isa::x86_avxvnni, isa::aarch64_pmull, isa::thumb2_crc, isa::riscv_v}) {
    if(!isa_available(preset)) {
      EXPECT_EQ(isa_detail::select(preset, preset), isa::generic);
      EXPECT_EQ((crc_dispatch_engine<std::uint32_t, crc_lut_sliced>::create(crc32_iso_hdlc, preset)->preset()), isa::generic);
    }
  }
  check_engines(isa_set{}, buffer);
}

// Each preset of this build named alone, as a caller that picks the preset itself does.
template <class Preset> using dispatch_presets = isa_test::preset_test<Preset>;
TYPED_TEST_SUITE(dispatch_presets, isa_test::presets, isa_test::preset_name);

TYPED_TEST(dispatch_presets, explicit_preset_matches_generic) {
  constexpr isa preset = TypeParam::value;
  EXPECT_EQ(isa_detail::select(preset, preset), preset);
  auto const buffer = random_bytes(70'008, 13);
  check_engines(preset, buffer);
  check_crcs(preset, buffer);
}

} // namespace
