#include "xnu-trace/xnu-trace.h"

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[EliasFano]"

TEST_CASE("build", TS) {
    const auto seq = get_random_sorted_unique_scalars<uint8_t>(16, 16);
    fmt::print("seq: {:d}\n", fmt::join(seq, ", "));
    EliasFanoSequence<sizeofbits<decltype(seq)::value_type>()> ef(std::span{seq});
}
