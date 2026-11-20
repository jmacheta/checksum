// Naming an instruction-set preset this build lacks fails the engine's requires-clause, not a link.
#include "test_engine.hpp"

#include <checksum/isa.hpp>

#include <string_view>

using namespace std::literals;

// A preset of another architecture.
constexpr checksum::isa preset = checksum::isa_available(checksum::isa::x86_pclmul) ? checksum::isa::riscv_v : checksum::isa::x86_pclmul;

static_assert(!checksum::isa_available(preset));

#ifdef CHECKSUM_EXPECT_COMPILE_ERROR
auto text_checksum() { return make_engine<preset>().compute("123456789"sv); }
#else
auto text_checksum() { return make_engine<checksum::isa::generic>().compute("123456789"sv); }
#endif
