// An accumulator cannot be created from a temporary engine: it stores the engine's address, which would dangle.
#include "test_engine.hpp"

#include <checksum/engine.hpp>
#include <checksum/isa.hpp>

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
auto accumulator = checksum::accumulator{make_engine<checksum::isa::generic>()};
#else
auto const engine = make_engine<checksum::isa::generic>();
auto accumulator = checksum::accumulator{engine};
#endif
