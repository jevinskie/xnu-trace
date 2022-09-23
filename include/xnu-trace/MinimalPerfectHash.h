#pragma once

#include "common.h"

#include <memory>

struct xxhash_64 {
    XNUTRACE_INLINE static uint64_t hash(uint64_t val);
    XNUTRACE_INLINE static uint64_t hash(uint64_t val, uint64_t seed);
};

struct xxhash3_64 {
    XNUTRACE_INLINE static uint64_t hash(uint64_t val);
    XNUTRACE_INLINE static uint64_t hash(uint64_t val, uint64_t seed);
};

template <typename KeyT, typename Hasher = xxhash3_64> class XNUTRACE_EXPORT MinimalPerfectHash {
public:
    MinimalPerfectHash<KeyT, Hasher>(std::vector<KeyT> keys);
    uint32_t lookup(KeyT key);

private:
    std::unique_ptr<int32_t[]> m_salts;
    size_t m_nkeys;
};
