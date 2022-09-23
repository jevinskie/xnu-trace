#include "xnu-trace/MinimalPerfectHash.h"
#include "common-internal.h"

#include "xnu-trace/XNUCommpageTime.h"

#include <algorithm>
#include <vector>

#define XXH_INLINE_ALL
#define XXH_NAMESPACE xnu_trace_mph_
#include <xxhash-xnu-trace/xxhash.h>

uint64_t xxhash_64::hash(uint64_t val) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), 0);
}

uint64_t xxhash_64::hash(uint64_t val, uint64_t seed) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

uint64_t xxhash3_64::hash(uint64_t val) {
    return XXH3_64bits_withSeed(reinterpret_cast<char const *>(&val), sizeof(val), 0);
}

uint64_t xxhash3_64::hash(uint64_t val, uint64_t seed) {
    return XXH3_64bits_withSeed(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

template <typename KeyT> struct bucket {
    uint32_t hmod;
    std::vector<KeyT> keys;
};

template <typename KeyT, typename Hasher>
void MinimalPerfectHash<KeyT, Hasher>::build(std::span<const KeyT> keys) {
    assert(keys.size() <= UINT32_MAX);
    m_nkeys = (uint32_t)keys.size();
    std::vector<KeyT> vkeys{keys.begin(), keys.end()};
    std::sort(vkeys.begin(), vkeys.end());
    vkeys.erase(std::unique(vkeys.begin(), vkeys.end()), vkeys.end());
    assert(vkeys.size() == m_nkeys && "keys for MPH are not unique");

    std::vector<bucket<KeyT>> buckets{m_nkeys};
    for (const auto &key : keys) {
        const uint32_t hmod = Hasher::hash(key) % m_nkeys;
        buckets[hmod].hmod  = hmod;
        buckets[hmod].keys.emplace_back(key);
    }

    std::sort(buckets.begin(), buckets.end(), [](const auto &a, const auto &b) {
        return a.keys.size() > b.keys.size();
    });

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
uint32_t MinimalPerfectHash<KeyT, Hasher>::operator()(KeyT key) const {
    const auto hmod     = Hasher::hash(key) % m_nkeys;
    const auto salt_val = m_salts[hmod];
    if (salt_val < 0) {
        return -salt_val - 1;
    } else {
        return Hasher::hash(key, salt_val) % m_nkeys;
    }
}

template class MinimalPerfectHash<uint64_t>;
