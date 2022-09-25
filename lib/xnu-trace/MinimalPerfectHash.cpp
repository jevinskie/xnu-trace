#include "xnu-trace/MinimalPerfectHash.h"
#include "common-internal.h"

#include "xnu-trace/XNUCommpageTime.h"

#include <algorithm>
#include <vector>

#include <range/v3/algorithm.hpp>
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

    std::vector<std::pair<KeyT, uint32_t>> colliding_hmods;

    std::vector<bucket<KeyT>> buckets{m_nkeys};
    for (const auto &key : keys) {
        const uint32_t hmod = Hasher::hash(key) % m_nkeys;
        buckets[hmod].hmod  = hmod;
        buckets[hmod].keys.emplace_back(key);
        if (key == 2820618731633718666ull || key == 15384097749270183390ull) {
            colliding_hmods.emplace_back(std::make_pair(key, hmod));
        }
    }

    for (const auto &[key, hmod] : colliding_hmods) {
        fmt::print("key: {:d} hmod {:d} bucket keys: {:d}\n", key, hmod,
                   fmt::join(buckets[hmod].keys, ", "));
    }

    // sort this way to match python impl
    std::sort(buckets.begin(), buckets.end(), [](const auto &a, const auto &b) {
        const auto sz_a = a.keys.size();
        const auto sz_b = b.keys.size();
        if (sz_a == sz_b) {
            return a.hmod < b.hmod;
        }
        return sz_a < sz_b;
    });
    std::reverse(buckets.begin(), buckets.end());

    fmt::print("({:d}, [{:d}])\n", buckets[0].hmod, fmt::join(buckets[0].keys, ", "));

    m_salts = std::make_unique<int32_t[]>(m_nkeys);
    std::vector<bool> slot_used(m_nkeys);

    XNUFastTimeout timeout{5'000'000'000, []() {
                               assert(!"mph construction timed out");
                           }};
    for (uint32_t i = 0; i < m_nkeys; ++i) {
        const auto hmod            = buckets[i].hmod;
        const auto bucket_num_keys = buckets[i].keys.size();
        if (bucket_num_keys > 1) {
            int32_t d = 1;
            while (true) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
                uint32_t salted_hashes[bucket_num_keys];
#pragma clang diagnostic pop
                const auto salted_hashes_end = salted_hashes + bucket_num_keys;
                std::fill(salted_hashes, salted_hashes_end, UINT32_MAX);
                bool all_free = true;
                for (size_t j = 0; j < bucket_num_keys; ++j) {
                    const auto shmod = Hasher::hash(buckets[i].keys[j], d) % m_nkeys;
                    if (std::find(salted_hashes, salted_hashes_end, shmod) != salted_hashes_end) {
                        fmt::print("got collison in salted hashes\n");
                        all_free = false;
                        break;
                    }
                    salted_hashes[j] = shmod;
                    all_free &= !slot_used[shmod];
                    if (!all_free) {
                        break;
                    }
                }
                if (all_free) {
                    for (size_t j = 0; j < bucket_num_keys; ++j) {
                        slot_used[salted_hashes[j]] = true;
                    }
                    m_salts[hmod] = d;
                    fmt::print("bucket idx: {:d} hmod: {:d} d: {:d}\n", i, hmod, d);
                    break;
                }
                ++d;
                timeout.check();
            }
        } else if (bucket_num_keys == 1) {
            auto it = std::find(slot_used.begin(), slot_used.end(), false);
            assert(it != slot_used.end());
            *it           = true;
            m_salts[hmod] = -std::distance(slot_used.begin(), it) - 1;
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

template <typename KeyT, typename Hasher> void MinimalPerfectHash<KeyT, Hasher>::stats() const {
    const auto max_d = ranges::max(std::span<int32_t>(m_salts.get(), m_nkeys));
    fmt::print("max_d: {:d}\n", max_d);
}

template class MinimalPerfectHash<uint64_t>;
