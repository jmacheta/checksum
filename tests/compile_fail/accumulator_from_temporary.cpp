// An accumulator cannot be created from a temporary engine: it stores the engine's address, which would dangle.
#include "test_engine.hpp"

#include <checksum/engine.hpp> // IWYU pragma: keep
#include <checksum/isa.hpp>

namespace {
#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
[[maybe_unused]] auto accumulator = checksum::accumulator{make_engine<checksum::isa::generic>()};
#else
auto const engine = make_engine<checksum::isa::generic>();
[[maybe_unused]] auto accumulator = checksum::accumulator{engine};
#endif
} // namespace
