#include "xnu-trace/xnu-trace.h"

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[memmem-chunking]"

TEST_CASE("smol-chunk", TS) {
    const uint8_t haystack[] = {1, 1, 1, 1};
    const uint8_t needle[]   = {1};
    const auto one_ptrs =
        chunk_into_bins_by_needle(4, haystack, sizeof(haystack), needle, sizeof(needle));
    REQUIRE(one_ptrs.size() == 4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(one_ptrs[i] == (void *)&haystack[i]);
    }
}

TEST_CASE("smol2-chunk", TS) {
    const uint8_t haystack[] = {1, 0, 1, 0, 1, 0, 1, 0};
    const uint8_t needle[]   = {1};
    const auto one_ptrs =
        chunk_into_bins_by_needle(4, haystack, sizeof(haystack), needle, sizeof(needle));
    REQUIRE(one_ptrs.size() == 4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(one_ptrs[i] == (void *)&haystack[i * 2]);
    }
}

TEST_CASE("smol2-uneven-chunk", TS) {
    const uint8_t haystack[] = {1, 0, 1, 0, 1, 1, 0, 0};
    const uint8_t needle[]   = {1};
    const auto one_ptrs =
        chunk_into_bins_by_needle(4, haystack, sizeof(haystack), needle, sizeof(needle));
    REQUIRE(one_ptrs.size() == 4);
    for (int i = 0; i < 4; ++i) {
        if (i != 3) {
            REQUIRE(one_ptrs[i] == (void *)&haystack[i * 2]);
        } else {
            REQUIRE(one_ptrs[i] == nullptr);
        }
    }
}
