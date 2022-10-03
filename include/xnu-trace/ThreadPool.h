#pragma once

#include "common.h"

#include "Atomic.h"

#include <BS_thread_pool.hpp>

class XNUTraceThreadPool : public BS::thread_pool {
public:
    void wait_on_n_tasks(size_t n, auto block) {
        AtomicWaiter waiter(n);
        for (size_t i = 0; i < n; ++i) {
            push_task([&, i] {
                block(i);
                waiter.release();
            });
        }
        waiter.wait();
    }
};

extern XNUTraceThreadPool xnutrace_pool;
