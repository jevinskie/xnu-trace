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

namespace std {
using atomic_uint128_t = std::atomic<uint128_t>;
using atomic_int128_t  = std::atomic<int128_t>;
} // namespace std
