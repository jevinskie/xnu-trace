#include "xnu-trace/xnu-trace.h"

#include <boost/ut.hpp>
#include <fmt/format.h>
#include <range/v3/algorithm.hpp>

using namespace boost::ut;

suite memmem_chunking_suite = [] {
    "smol_chunk"_test = [] {
        const uint8_t haystack[] = {1, 1, 1, 1};
        const uint8_t needle[]   = {1};
        const auto one_ptrs =
            chunk_into_bins_by_needle(4, haystack, sizeof(haystack), needle, sizeof(needle));
        expect(one_ptrs.size() == 4);
        for (int i = 0; i < 4; ++i) {
            expect(one_ptrs[i] == (void *)&haystack[i]);
        }
    };
};
