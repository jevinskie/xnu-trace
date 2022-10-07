#include "xnu-trace/xnu-trace.h"

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[AtomicBitvector]"

TEST_CASE("buf_size", TS) {
    auto ebv = BitVector<32, false>{32, 0};
    REQUIRE(ebv.buf_size(32, 4) == 4);
}

TEST_CASE("build_bv", TS) {
    auto bv = BitVector<32, false>{31, 243};
}

TEST_CASE("build_abv", TS) {
    auto abv = AtomicBitVector<32, false>{31, 243};
}
