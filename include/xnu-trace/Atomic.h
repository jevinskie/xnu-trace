#pragma once

#include "common.h"

#include <atomic>
#include <cstdint>

template <typename T> class AtomicWaiter {
public:
    AtomicWaiter(T max) : m_max{max} {};
    void increment(T num = 1) {
        m_cnt += num;
        m_cnt.notify_all();
    }
    void wait() {
        T cur = m_cnt;
        while (cur != m_max) {
            m_cnt.wait(cur);
            cur = m_cnt;
        }
    }

private:
    T m_max;
    std::atomic<T> m_cnt{};
};
