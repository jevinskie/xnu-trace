#pragma once

#include "common.h"

#include "Atomic.h"

#include <type_traits>

#include <BS_thread_pool.hpp>

class XNUTraceThreadPool : public BS::thread_pool {
public:
    void wait_on_n_tasks(size_t n, const auto &block) {
        AtomicWaiter waiter(n);
        for (size_t i = 0; i < n; ++i) {
            push_task([&, i] {
                block(i);
                waiter.release();
            });
        }
        waiter.wait();
    }

    template <typename F, typename Idx1, typename Idx2,
              typename Idx = std::common_type_t<Idx1, Idx2>>
    std::enable_if_t<std::is_invocable_v<F, size_t, Idx, Idx>>
    parallelize_indexed_loop(const Idx1 first_index, const Idx2 index_after_last, const F &loop,
                             const size_t num_blocks = 0) {
        BS::blocks blks(first_index, index_after_last,
                        num_blocks ? num_blocks : get_thread_count());
        if (!blks.get_total_size()) {
            return;
        }
        const auto num_blks = blks.get_num_blocks();
        AtomicWaiter waiter(num_blks);
        for (size_t i = 0; i < num_blks; ++i) {
            push_task([&, i] {
                loop(i, blks.start(i), blks.end(i));
                waiter.release();
            });
        }
        waiter.wait();
    }

    template <typename F, typename Idx>
    std::enable_if_t<std::is_invocable_v<F, size_t, Idx, Idx>>
    parallelize_indexed_loop(const Idx index_after_last, const F &loop,
                             const size_t num_blocks = 0) {
        parallelize_indexed_loop(0, index_after_last, loop, num_blocks);
    }
};

extern XNUTraceThreadPool xnutrace_pool;
