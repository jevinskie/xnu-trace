#include "xnu-trace/xnu-trace.h"

#include <cstdlib>
#include <filesystem>

#include <boost/ut.hpp>

namespace fs = std::filesystem;
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
        const auto keys = get_random_uints(100'000);
        MinimalPerfectHash<uint64_t> mph;
        mph.build(keys);
    };

    "check"_test = [] {
        const auto keys = get_random_uints(100'000);
        MinimalPerfectHash<uint64_t> mph;
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

    "check_page_addrs"_test = [] {
        const auto buf =
            read_file(fs::path(__FILE__).parent_path().parent_path() / "test" / "page-addrs.bin");
        const auto raw_buf = (uint64_t *)buf.data();
        const auto nkeys   = buf.size() / sizeof(uint64_t);
        std::vector<uint64_t> keys;
        keys.reserve(nkeys);
        for (size_t i = 0; i < nkeys; ++i) {
            keys.emplace_back(raw_buf[i]);
        }
        MinimalPerfectHash<uint64_t> mph;
        mph.build(keys);
        mph.stats();
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
