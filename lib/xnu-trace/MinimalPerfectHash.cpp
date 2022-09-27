#include "xnu-trace/MinimalPerfectHash.h"
#include "common-internal.h"

#include "xnu-trace/XNUCommpageTime.h"

#include <algorithm>
#include <vector>

#include <fastmod.h>
#include <range/v3/algorithm.hpp>
#define XXH_INLINE_ALL
#define XXH_NAMESPACE xnu_trace_mph_
#include <xxhash-xnu-trace/xxhash.h>

xxhash_64::type xxhash_64::hash(uint64_t val, uint64_t seed) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

xxhash_32::type xxhash_32::hash(uint64_t val, uint64_t seed) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

xxhash3_64::type xxhash3_64::hash(uint64_t val, uint64_t seed) {
    return XXH3_64bits_withSeed(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

xxhash3_32::type xxhash3_32::hash(uint64_t val, uint64_t seed) {
    return XXH3_64bits_withSeed(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

jevhash_64::type jevhash_64::hash(uint64_t val, uint64_t seed) {
    uint64_t acc = seed + 1;
    acc *= XXH_PRIME64_2;
    acc ^= val;
    acc += seed;
    acc = XXH_rotl64(acc, 13);
    return acc;
}

jevhash_32::type jevhash_32::hash(uint64_t val, uint64_t seed) {
    uint32_t acc = seed + 1;
    acc *= XXH_PRIME32_2;
    acc ^= (val >> 32);
    acc += seed;
    acc ^= (val & 0xFFFF'FFFF);
    acc = XXH_rotl32(acc, 13);
    return acc;
}

template <typename KeyT> struct bucket {
    uint32_t hmod;
    std::vector<KeyT> keys;
};

template <typename KeyT, typename Hasher>
uint32_t MinimalPerfectHash<KeyT, Hasher>::mod(typename Hasher::type n) const {
    if constexpr (std::is_same_v<typename Hasher::type, uint32_t>) {
        return fastmod::fastmod_u32(n, m_fastmod_u32_M, m_nkeys);
    } else {
        return n % m_nkeys;
    }
}

template <typename KeyT, typename Hasher>
void MinimalPerfectHash<KeyT, Hasher>::build(std::span<const KeyT> keys) {
    assert(keys.size() <= UINT32_MAX);
    m_nkeys = (uint32_t)keys.size();
    if constexpr (std::is_same_v<typename Hasher::type, uint32_t>) {
        m_fastmod_u32_M = fastmod::computeM_u32(m_nkeys);
    }
    std::vector<KeyT> vkeys{keys.begin(), keys.end()};
    std::sort(vkeys.begin(), vkeys.end());
    vkeys.erase(std::unique(vkeys.begin(), vkeys.end()), vkeys.end());
    assert(vkeys.size() == m_nkeys && "keys for MPH are not unique");

    std::vector<bucket<KeyT>> buckets{m_nkeys};
    for (const auto &key : keys) {
        const auto hmod    = mod(Hasher::hash(key));
        buckets[hmod].hmod = hmod;
        buckets[hmod].keys.emplace_back(key);
    }

    std::sort(buckets.begin(), buckets.end(), [](const auto &a, const auto &b) {
        return a.keys.size() > b.keys.size();
    });

    // fmt::print("({:d}, [{:d}])\n", buckets[0].hmod, fmt::join(buckets[0].keys, ", "));

    m_salts.resize(m_nkeys);
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
                    const auto shmod = mod(Hasher::hash(buckets[i].keys[j], d));
                    if (std::find(salted_hashes, salted_hashes_end, shmod) != salted_hashes_end) {
                        // collision within salted hashes, try again
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
    const auto hmod     = mod(Hasher::hash(key));
    const auto salt_val = m_salts[hmod];
    if (salt_val < 0) {
        return -salt_val - 1;
    } else {
        return mod(Hasher::hash(key, salt_val));
    }
}

template <typename KeyT, typename Hasher> void MinimalPerfectHash<KeyT, Hasher>::stats() const {
    const auto max_d = ranges::max(m_salts);
    fmt::print("max_d: {:d}\n", max_d);
    const auto num_empty = ranges::count(m_salts, 0);
    fmt::print("empty: {:0.3f}%\n", num_empty * 100.0 / m_nkeys);
}

template class MinimalPerfectHash<uint8_t>;
template class MinimalPerfectHash<uint16_t>;
template class MinimalPerfectHash<uint32_t>;
template class MinimalPerfectHash<uint64_t>;
