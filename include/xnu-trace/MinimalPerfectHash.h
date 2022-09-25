#pragma once

#include "common.h"

#include <memory>
#include <span>

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
    void build(std::span<const KeyT> keys);
    XNUTRACE_INLINE uint32_t operator()(KeyT key) const;
    void stats() const;

private:
    std::unique_ptr<int32_t[]> m_salts;
    uint32_t m_nkeys;
};
