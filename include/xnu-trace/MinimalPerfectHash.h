#pragma once

#include "common.h"

#include <span>
#include <vector>

struct xxhash_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

struct xxhash_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

struct xxhash3_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

struct xxhash3_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

struct jevhash_64 {
    using type = uint64_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

struct jevhash_32 {
    using type = uint32_t;
    XNUTRACE_INLINE static type hash(uint64_t val, uint64_t seed = 0) noexcept;
};

template <typename KeyT, typename Hasher = jevhash_32> class XNUTRACE_EXPORT MinimalPerfectHash {
public:
    void build(std::span<const KeyT> keys);
    XNUTRACE_INLINE uint32_t operator()(KeyT key) const;
    void stats() const;

private:
    XNUTRACE_INLINE uint32_t mod(typename Hasher::type n) const;

    std::vector<int32_t> m_salts;
    uint64_t m_fastmod_u32_M;
    uint32_t m_nkeys;
};

template <typename KeyT, typename ValueT, typename Hasher = jevhash_32>
class XNUTRACE_EXPORT mph_map_static {
public:
    mph_map_static() = default;
    mph_map_static(const std::vector<std::pair<KeyT, ValueT>> &key_vals) {
        build(key_vals);
    }

    void build(const std::vector<std::pair<KeyT, ValueT>> &key_vals) {
        std::vector<KeyT> keys(key_vals.size());
        size_t i = 0;
        for (const auto &[k, v] : key_vals) {
            keys[i] = k;
            ++i;
        }
        m_mph.build(keys);
        m_values.resize(key_vals.size());
        for (const auto &[k, v] : key_vals) {
            m_values[m_mph(k)] = v;
        }
    }

    XNUTRACE_INLINE const ValueT &operator[](KeyT key) const {
        return m_values[m_mph(key)];
    }

private:
    MinimalPerfectHash<KeyT, Hasher> m_mph;
    std::vector<ValueT> m_values;
};

template <typename KeyT, typename ValueT, typename Hasher = jevhash_32>
class XNUTRACE_EXPORT mph_map {
public:
    XNUTRACE_INLINE ValueT &operator[](KeyT key) {
        if (XNUTRACE_UNLIKELY(m_key_vals.size() == 0)) {
            m_key_vals.emplace_back(std::make_pair(key, ValueT{}));
            m_mph.build(std::span<KeyT>{&key, 1});
            return m_key_vals[0].second;
        }
        auto &[k, v] = m_key_vals[m_mph(key)];
        if (XNUTRACE_LIKELY(k == key)) {
            return v;
        }
        m_key_vals.emplace_back(std::make_pair(key, ValueT{}));
        rebuild();
        return m_key_vals[m_mph(key)].second;
    }

    bool contains(KeyT key) const {
        if (XNUTRACE_UNLIKELY(m_key_vals.size() == 0)) {
            return false;
        }
        return m_key_vals[m_mph(key)].first == key;
    }

    template <typename... Args>
    std::pair<ValueT &, bool> try_emplace(const KeyT &key, Args &&...args) {
        if (XNUTRACE_UNLIKELY(contains(key))) {
            return {m_key_vals[m_mph(key)].second, false};
        }
        m_key_vals.emplace_back(
            std::pair<KeyT, ValueT>(std::piecewise_construct, std::forward_as_tuple(key),
                                    std::forward_as_tuple(std::forward<Args>(args)...)));
        rebuild();
        return {m_key_vals[m_mph(key)].second, true};
    }

    std::vector<KeyT> keys() const {
        std::vector<KeyT> res(m_key_vals.size());
        size_t i = 0;
        for (const auto &[k, v] : m_key_vals) {
            res[i] = k;
            ++i;
        }
        return res;
    }

    std::pair<KeyT, ValueT> *begin() {
        return &*m_key_vals.begin();
    }
    const std::pair<KeyT, ValueT> *begin() const {
        return cbegin();
    }
    const std::pair<KeyT, ValueT> *cbegin() const {
        return &*m_key_vals.cbegin();
    }
    std::pair<KeyT, ValueT> *end() {
        return &*m_key_vals.end();
    }
    const std::pair<KeyT, ValueT> *end() const {
        return cend();
    }
    const std::pair<KeyT, ValueT> *cend() const {
        return &*m_key_vals.cend();
    }

private:
    void rebuild() {
        m_mph.build(keys());
        std::vector<uint32_t> new_idx(m_key_vals.size());
        size_t i = 0;
        for (const auto &[k, v] : m_key_vals) {
            new_idx[i] = m_mph(k);
            ++i;
        }
        permute(new_idx);
    }

    // everybody loves raymond https://devblogs.microsoft.com/oldnewthing/20170102-00/?p=95095
    void permute(std::vector<uint32_t> &new_idx) {
        for (uint32_t i = 0; i < new_idx.size(); i++) {
            std::pair<KeyT, ValueT> t{std::move(m_key_vals[i])};
            auto current = i;
            while (i != new_idx[current]) {
                auto next           = new_idx[current];
                m_key_vals[current] = std::move(m_key_vals[next]);
                new_idx[current]    = current;
                current             = next;
            }
            m_key_vals[current] = std::move(t);
            new_idx[current]    = current;
        }
    }

    MinimalPerfectHash<KeyT, Hasher> m_mph;
    std::vector<std::pair<KeyT, ValueT>> m_key_vals;
};
