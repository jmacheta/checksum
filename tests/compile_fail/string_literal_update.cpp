// byte_range rejects string literals, since their terminating '\0' would be hashed.
#include "test_engine.hpp"

#include <checksum/isa.hpp>

#include <string_view>

using namespace std::literals;

auto const engine = make_engine<checksum::isa::generic>();

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
[[maybe_unused]] auto const test_state = engine.update(engine.initial(), "123456789");
#else
[[maybe_unused]] auto const test_state = engine.update(engine.initial(), "123456789"sv);
#endif
