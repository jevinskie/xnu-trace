#undef NDEBUG
#include <cassert>
#include <csignal>
#include <cstdint>
#include <unistd.h>

#include <mach/mach_init.h>
#include <mach/mach_traps.h>

#include <argparse/argparse.hpp>
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

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());
    parser.add_argument("-c", "--crash-on-attach")
        .help("crash on attachment")
        .default_value(false)
        .implicit_value(true);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    const auto crash_on_attach = parser["--crash-on-attach"] == true;

    uint64_t num_yields  = 0;
    const auto task_self = mach_task_self();
    fmt::print("yield-loop-step-test begin\n");
    setup_sig_handler();
    should_stop = false;
    while (!should_stop) {
        ++num_yields;
        // swtch_pri(0);
        fmt::print("num_yields: {:d}\n", num_yields);
        usleep(1'000'000);
        if (crash_on_attach && get_task_for_pid_count(task_self)) {
            null_deref();
        }
    }
    fmt::print("num_yields: {:d}\n", num_yields);
    fmt::print("yield-loop-step-test end\n");
    return 0;
}
