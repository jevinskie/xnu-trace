#include "xnu-trace/MinimalPerfectHash.h"
#include "common-internal.h"

#include "xnu-trace/XNUCommpageTime.h"

#include <algorithm>
#include <vector>

#define XXH_INLINE_ALL
#define XXH_NAMESPACE xnu_trace_mph_
#include <xxhash.h>

uint64_t xxhash_64::hash(uint64_t val, uint64_t seed = 0) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

template <typename KeyT> struct bucket {
    uint32_t hmod;
    std::vector<KeyT> keys;
};

template <typename KeyT, typename Hasher>
MinimalPerfectHash<KeyT, Hasher>::MinimalPerfectHash(std::vector<KeyT> keys) {
    assert(keys.size() <= UINT32_MAX);
    m_nkeys = (uint32_t)keys.size();
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    assert(keys.size() == m_nkeys && "keys for MPH are not unique");

    std::vector<bucket<KeyT>> buckets{m_nkeys};
    for (const auto &key : keys) {
        const auto hash    = Hasher::hash(key);
        uint32_t hmod      = hash % m_nkeys;
        buckets[hmod].hmod = hmod;
        buckets[hmod].keys.emplace_back(key);
    }

    std::sort(buckets.begin(), buckets.end(), [](const auto &a, const auto &b) {
        return a.keys.size() > b.keys.size();
    });
    fmt::print("bucket[0] hmod: {:#010x} sz: {:d}\n", buckets[0].hmod, buckets[0].keys.size());

    m_salts = std::make_unique<int32_t[]>(m_nkeys);
    std::vector<bool> slot_used(m_nkeys);

    XNUFastTimeout timeout{5'000'000'000, []() {
                               assert(!"mph construction timed out");
                           }};
    for (uint32_t i = 0; i < m_nkeys; ++i) {
        const auto hmod            = buckets[i].hmod;
        const auto bucket_num_keys = (int)buckets[i].keys.size();
        if (bucket_num_keys > 1) {
            int32_t d = 1;
            while (true) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
                uint32_t salted_hashes[bucket_num_keys];
#pragma clang diagnostic pop
                bool all_free = true;
                for (int j = 0; j < bucket_num_keys; ++j) {
                    const auto shmod = Hasher::hash(hmod, d) % m_nkeys;
                    salted_hashes[j] = shmod;
                    all_free &= !slot_used[shmod];
                    if (!all_free) {
                        break;
                    }
                }
                if (all_free) {
                    for (int j = 0; j < bucket_num_keys; ++j) {
                        slot_used[salted_hashes[j]] = true;
                    }
                    m_salts[hmod] = d;
                    break;
                }
                ++d;
                timeout.check();
            }
        } else if (bucket_num_keys == 1) {
            const auto free_idx = std::distance(
                slot_used.cbegin(), std::find(slot_used.cbegin(), slot_used.cend(), false));
            slot_used[free_idx] = true;
            m_salts[hmod]       = -free_idx - 1;
        }
    }
}

template <typename KeyT, typename Hasher>
uint32_t MinimalPerfectHash<KeyT, Hasher>::lookup(KeyT key) {
    return 0;
}

template class MinimalPerfectHash<uint64_t>;
