#pragma once

#include "common.h"

#include <memory>
#include <span>

struct xxhash_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

struct xxhash_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

struct xxhash3_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

struct xxhash3_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

struct jevhash_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

struct jevhash_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0);
};

template <typename KeyT, typename Hasher = jevhash_32> class XNUTRACE_EXPORT MinimalPerfectHash {
public:
    void build(std::span<const KeyT> keys);
    XNUTRACE_INLINE uint32_t operator()(KeyT key) const;
    void stats() const;

private:
    XNUTRACE_INLINE uint32_t mod(typename Hasher::type n) const;

    std::unique_ptr<int32_t[]> m_salts;
    uint64_t m_fastmod_u32_M;
    uint32_t m_nkeys;
};
