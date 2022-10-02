#include "xnu-trace/xnu-trace.h"

#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[MinimalPerfectHash]"

namespace fs = std::filesystem;

template <typename KeyT>
static void check_mph(const std::vector<KeyT> &keys, const MinimalPerfectHash<KeyT> &mph) {
    std::vector<uint32_t> idxes;
    idxes.reserve(keys.size());
    for (const auto &k : keys) {
        idxes.emplace_back(mph(k));
    }
    std::sort(idxes.begin(), idxes.end());
    idxes.erase(std::unique(idxes.begin(), idxes.end()), idxes.end());
    REQUIRE(idxes.size() == keys.size());
    REQUIRE(idxes[0] == 0);
    REQUIRE(idxes[idxes.size() - 1] == idxes.size() - 1);
}

TEST_CASE("build", TS) {
    const auto keys = get_random_scalars<uint64_t>(100'000);
    MinimalPerfectHash<uint64_t> mph;
    mph.build(keys);
}

TEST_CASE("check", TS) {
    const auto keys = get_random_scalars<uint64_t>(100'000);
    MinimalPerfectHash<uint64_t> mph;
    mph.build(keys);
    check_mph(keys, mph);
}

TEST_CASE("check_page_addrs", TS) {
    const auto keys = read_numbers_from_file<uint64_t>(
        fs::path(__FILE__).parent_path().parent_path() / "test" / "page_addrs.bin");
    MinimalPerfectHash<uint64_t> mph;
    mph.build(keys);
    check_mph(keys, mph);
}

TEST_CASE("check_rand_u64_dup_idx_29751", TS) {
    const auto keys = read_numbers_from_file<uint64_t>(
        fs::path(__FILE__).parent_path().parent_path() / "test" / "rand_u64_dup_idx_29751.bin");
    MinimalPerfectHash<uint64_t> mph;
    mph.build(keys);
    check_mph(keys, mph);
}
