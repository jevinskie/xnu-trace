#include <boost/ut.hpp>

#include "xnu-trace/xnu-trace.h"

#include <cstdlib>

using namespace boost::ut;

std::vector<uint64_t> get_random_uints(size_t n) {
    std::vector<uint64_t> res;
    res.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        uint64_t r;
        arc4random_buf((uint8_t *)&r, sizeof(r));
        res.emplace_back(r);
    }
    return res;
}

suite mph_suite = [] {
    "build"_test = [] {
        MinimalPerfectHash<uint64_t> mph;
        const auto keys = get_random_uints(100'000);
        mph.build(keys);
    };

    "check"_test = [] {
        MinimalPerfectHash<uint64_t> mph;
        const auto keys = get_random_uints(100'000);
        mph.build(keys);
        std::vector<uint32_t> idxes;
        idxes.reserve(keys.size());
        for (const auto &k : keys) {
            idxes.emplace_back(mph(k));
        }
        std::sort(idxes.begin(), idxes.end());
        idxes.erase(std::unique(idxes.begin(), idxes.end()), idxes.end());
        expect(idxes.size() == keys.size());
        expect(idxes[0] == 0);
        expect(idxes[idxes.size() - 1] == idxes.size() - 1);
    };
};
