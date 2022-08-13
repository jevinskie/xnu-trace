#undef NDEBUG
#include <cassert>
#include <csignal>
#include <cstdint>
#include <unistd.h>

#include <mach/mach_traps.h>

#include <fmt/format.h>

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

int main() {
    uint64_t num_yields = 0;
    fmt::print("yield-loop-step-test begin\n");
    setup_sig_handler();
    should_stop = false;
    while (!should_stop) {
        ++num_yields;
        // swtch_pri(0);
        usleep(1'000);
    }
    fmt::print("num_yields: {:d}\n", num_yields);
    fmt::print("yield-loop-step-test end\n");
    return 0;
}
