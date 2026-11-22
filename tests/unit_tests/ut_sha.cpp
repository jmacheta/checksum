// SHA-1 and SHA-2: FIPS 180-4 examples, reference values, splits and byte ranges, at compile time and at run time.

#include <checksum/engine.hpp>
#include <checksum/sha.hpp>
#include <sha_reference_vectors.hpp>

#include <engine_checks.hpp>
#include <gtest/gtest.h>
#include <test_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace checksum;
using namespace std::literals;
using enum sha_variant;

constexpr std::array<std::byte, sha_test::message_size> message = [] {
  std::array<std::byte, sha_test::message_size> result{};
  fill_random(result);
  return result;
}();

// The digest written as hex digits.
template <std::size_t Size> constexpr std::array<std::uint8_t, Size> digest(std::string_view hex) {
  auto const nibble = [](char digit) { return static_cast<std::uint8_t>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
  std::array<std::uint8_t, Size> result{};
  for(std::size_t index = 0; index < Size; ++index) {
    result[index] = static_cast<std::uint8_t>((nibble(hex[2 * index]) << 4U) | nibble(hex[(2 * index) + 1]));
  }
  return result;
}

// The examples of FIPS 180-4 (NIST's "abc" and two-block messages).
static_assert(sha_compute<sha1>("abc"sv) == digest<20>("a9993e364706816aba3e25717850c26c9cd0d89d"));
static_assert(sha_compute<sha224>("abc"sv) == digest<28>("23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"));
static_assert(sha_compute<sha256>("abc"sv) == digest<32>("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
static_assert(sha_compute<sha384>("abc"sv) ==
              digest<48>("cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"));
static_assert(sha_compute<sha512>("abc"sv) ==
              digest<64>("ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd"
                         "454d4423643ce80e2a9ac94fa54ca49f"));
static_assert(sha_compute<sha512_224>("abc"sv) == digest<28>("4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa"));
static_assert(sha_compute<sha512_256>("abc"sv) == digest<32>("53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"));
static_assert(sha_compute<sha256>("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"sv) ==
              digest<32>("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
static_assert(sha_compute<sha1>(""sv) == digest<20>("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
static_assert(engine<sha_engine<sha256>> && engine<sha_dispatch_engine<sha512, sha_unrolled>> && !resumable_engine<sha_engine<sha1>>);
static_assert(sha_compute<sha512, sha_unrolled>("abc"sv) == sha_compute<sha512>("abc"sv) && !sha_strategy<int>);
static_assert(sha_dispatch_engine<sha384>(isa_set{}).preset() == isa::generic);

template <class Variant> class sha : public testing::Test {};

using variants = testing::Types<std::integral_constant<sha_variant, sha1>, std::integral_constant<sha_variant, sha224>,
                                std::integral_constant<sha_variant, sha256>, std::integral_constant<sha_variant, sha384>,
                                std::integral_constant<sha_variant, sha512>, std::integral_constant<sha_variant, sha512_224>,
                                std::integral_constant<sha_variant, sha512_256>>;

// Names the test instances after the variant.
struct variant_names {
  // NOLINTNEXTLINE(readability-identifier-naming): name required by Google Test.
  template <class Variant> static std::string GetName(int /*index*/) {
    constexpr std::array names{"sha1", "sha224", "sha256", "sha384", "sha512", "sha512_224", "sha512_256"};
    return names[static_cast<std::size_t>(Variant::value)];
  }
};

TYPED_TEST_SUITE(sha, variants, variant_names);

// Calls check.template operator()<Strategy>() for both strategies.
template <class Check> void for_each_strategy(Check const &check) {
  check.template operator()<sha_compact>();
  check.template operator()<sha_unrolled>();
}

TYPED_TEST(sha, reference_values) {
  constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  for_each_strategy([]<class Strategy>() {
    for(std::size_t index = 0; index < sha_test::prefix_sizes.size(); ++index) {
      std::size_t const size = sha_test::prefix_sizes[index];
      auto const data = std::span(message).first(size);
      EXPECT_EQ((sha_compute<variant, Strategy>(data)), vectors::prefixes[index]) << "size " << size;
      EXPECT_EQ((sha_dispatch_engine<variant, Strategy>{}.compute(data)), vectors::prefixes[index]) << "size " << size;
    }
  });
}

// The prefixes up to 200 bytes, computed during constant evaluation with both strategies.
TYPED_TEST(sha, constant_evaluation) {
  static constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  for_each_strategy([]<class Strategy>() {
    static constexpr auto computed = [] {
      std::array<typename vectors::digest, 18> result{};
      for(std::size_t index = 0; index < result.size(); ++index) {
        result[index] = sha_compute<variant, Strategy>(std::span(message).first(sha_test::prefix_sizes[index]));
      }
      return result;
    }();
    static_assert(sha_test::prefix_sizes[computed.size() - 1] == 200);
    for(std::size_t index = 0; index < computed.size(); ++index) {
      EXPECT_EQ(computed[index], vectors::prefixes[index]) << "size " << sha_test::prefix_sizes[index];
    }
    static_assert(engine_checks::chunked(sha_engine<variant, Strategy>{}, std::span(message).first(200), 7) == vectors::prefixes[17]);
  });
}

TYPED_TEST(sha, split_anywhere) {
  for_each_strategy([]<class Strategy>() {
    engine_checks::check_split_anywhere(sha_engine<TypeParam::value, Strategy>{}, std::span(message).first(300));
    EXPECT_TRUE(engine_checks::same_state_for_any_split(sha_engine<TypeParam::value, Strategy>{}, std::span(message).first(260)));
  });
}

// A state is the same for both strategies: either may continue and finish what the other started.
TYPED_TEST(sha, strategies_share_the_state) {
  constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  sha_engine<variant, sha_compact> const compact;
  sha_engine<variant, sha_unrolled> const unrolled;
  for(std::size_t const split : {0UZ, 1UZ, 64UZ, 129UZ, 999UZ, 1000UZ}) {
    auto const first = std::span(message).first(split);
    auto const rest = std::span(message).subspan(split);
    EXPECT_EQ(unrolled.finalize(unrolled.update(compact.update(compact.initial(), first), rest)), vectors::prefixes.back()) << "split " << split;
    EXPECT_EQ(compact.finalize(compact.update(unrolled.update(unrolled.initial(), first), rest)), vectors::prefixes.back()) << "split " << split;
  }
}

// Pieces of every size up to past two blocks, so that splits fall at every offset around the block and padding boundaries.
TYPED_TEST(sha, split_into_chunks) {
  constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  for_each_strategy([]<class Strategy>() {
    for(std::size_t chunk_size = 1; chunk_size <= 260; ++chunk_size) {
      ASSERT_EQ(engine_checks::chunked(sha_engine<variant, Strategy>{}, message, chunk_size), vectors::prefixes.back()) << "chunk " << chunk_size;
    }
  });
}

TYPED_TEST(sha, byte_ranges) {
  constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  constexpr std::string_view text = "123456789 123456789 123456789 123456789 123456789 123456789 123456789";
  auto const expected = sha_compute<variant>(std::as_bytes(std::span(text)));
  EXPECT_EQ(sha_compute<variant>(text), expected);
  EXPECT_EQ(sha_compute<variant>(std::list<char>(text.begin(), text.end())), expected);
  EXPECT_EQ(sha_compute<variant>(std::vector<unsigned char>(text.begin(), text.end())), expected);
  // Longer than one 64-byte chunk of a non-contiguous range.
  EXPECT_EQ(sha_compute<variant>(std::list<std::byte>(message.begin(), message.end())), vectors::prefixes.back());
  // A digest is a range of bytes itself.
  EXPECT_EQ(sha_compute<variant>(expected), sha_compute<variant>(std::as_bytes(std::span(expected))));
}

TYPED_TEST(sha, long_messages) {
  constexpr sha_variant variant = TypeParam::value;
  using vectors = sha_test::vector_set<variant>;
  auto const data = random_bytes(sha_test::long_message_size, 5);
  std::vector<char> const a(1'000'000, 'a');
  for_each_strategy([&]<class Strategy>() {
    EXPECT_EQ((sha_compute<variant, Strategy>(data)), vectors::long_message);
    EXPECT_EQ(engine_checks::chunked(sha_engine<variant, Strategy>{}, data, 4099), vectors::long_message);
    EXPECT_EQ((sha_compute<variant, Strategy>(a)), vectors::million_a);
  });
}

} // namespace
