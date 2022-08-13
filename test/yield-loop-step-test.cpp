#undef NDEBUG
#include <cassert>
#include <csignal>
#include <cstdint>
#include <unistd.h>

#include <mach/mach_init.h>
#include <mach/mach_traps.h>

#include <fmt/format.h>

#include "xnu-single-step-trace/xnu-single-step-trace.h"

static volatile bool should_stop;

static void sig_handler(int sig_id) {
    (void)sig_id;
    should_stop = true;
}

static void setup_sig_handler() {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}

__attribute__((noinline)) static void null_deref() {
    int *pnull = nullptr;
    *pnull     = 1337;
}

int main() {
    uint64_t num_yields  = 0;
    const auto task_self = mach_task_self();
    fmt::print("yield-loop-step-test begin\n");
    setup_sig_handler();
    should_stop = false;
    while (!should_stop) {
        ++num_yields;
        // swtch_pri(0);
        usleep(1'000);
        if (get_task_for_pid_count(task_self)) {
            null_deref();
        }
    }
    fmt::print("num_yields: {:d}\n", num_yields);
    fmt::print("yield-loop-step-test end\n");
    return 0;
}
