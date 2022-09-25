#include "xnu-trace/xnu-trace.h"

#include <cstdlib>
#include <filesystem>

#include <boost/ut.hpp>
#include <fmt/format.h>
#include <range/v3/algorithm.hpp>

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

template <typename KeyT>
static void check_mph(const std::vector<KeyT> &keys, const MinimalPerfectHash<KeyT> &mph) {
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
}

template <typename T> std::vector<T> read_numbers_from_file(const fs::path &path) {
    const auto buf     = read_file(path);
    const auto raw_buf = (T *)buf.data();
    const auto nnums   = buf.size() / sizeof(T);
    std::vector<T> nums;
    nums.reserve(nnums);
    for (size_t i = 0; i < nnums; ++i) {
        nums.emplace_back(raw_buf[i]);
    }
    return nums;
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
        check_mph(keys, mph);
    };

    "check_page_addrs"_test = [] {
        const auto keys = read_numbers_from_file<uint64_t>(
            fs::path(__FILE__).parent_path().parent_path() / "test" / "page_addrs.bin");
        MinimalPerfectHash<uint64_t> mph;
        mph.build(keys);
        check_mph(keys, mph);
    };

    "check_rand_u64_dup_idx_29751"_test = [] {
        const auto keys = read_numbers_from_file<uint64_t>(
            fs::path(__FILE__).parent_path().parent_path() / "test" / "rand_u64_dup_idx_29751.bin");
        MinimalPerfectHash<uint64_t> mph;
        mph.build(keys);
        check_mph(keys, mph);
    };
};
