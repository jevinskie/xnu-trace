#pragma once

#include "common.h"

#include <atomic>
#include <concepts>
#include <cstdint>

template <typename T> class AtomicWaiter {
public:
    AtomicWaiter(T count) : m_cnt{count} {};
    void release(T num = 1) {
        m_cnt -= num;
        m_cnt.notify_all();
    }
    void wait() {
        T cur = m_cnt;
        while (cur) {
            m_cnt.wait(cur);
            cur = m_cnt;
        }
    }

private:
    std::atomic<T> m_cnt;
};

template <typename T>
requires requires(T o) {
    requires sizeof(T) == sizeof(uint128_t);
    requires !std::same_as<T, uint128_t>;
    requires std::is_trivially_copyable_v<T>;
}
class std::atomic<T> : public atomic<uint128_t> {
public:
    using value_type      = T;
    using difference_type = value_type;

public:
    XNUTRACE_INLINE value_type
    load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
        const uint128_t res = atomic<uint128_t>::load(order);
        return *(value_type *)&res;
    }

    XNUTRACE_INLINE value_type load(std::memory_order order = std::memory_order_seq_cst) const
        volatile noexcept;

    XNUTRACE_INLINE operator value_type() const noexcept {
        const uint128_t res = atomic<uint128_t>::load();
        return *(value_type *)&res;
    }

    XNUTRACE_INLINE operator value_type() const volatile noexcept;

    XNUTRACE_INLINE value_type operator=(value_type desired) noexcept {
        return atomic<uint128_t>::operator=(*(uint128_t *)&desired);
    }

    XNUTRACE_INLINE value_type operator=(value_type desired) volatile noexcept;

    XNUTRACE_INLINE void store(value_type desired,
                               std::memory_order order = std::memory_order_seq_cst) noexcept {
        atomic<uint128_t>::store(*(uint128_t *)&desired, order);
    }

    XNUTRACE_INLINE void
    store(value_type desired,
          std::memory_order order = std::memory_order_seq_cst) volatile noexcept;
};

namespace std {
using atomic_uint128_t = std::atomic<uint128_t>;
using atomic_int128_t  = std::atomic<int128_t>;
} // namespace std
