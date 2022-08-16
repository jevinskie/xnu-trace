#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <unistd.h>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include <argparse/argparse.hpp>
#include <fmt/core.h>

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());
    parser.add_argument("--spawn").help("spawn process");
    parser.add_argument("--attach").help("attach to process");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    bool do_spawn  = parser.present("--spawn") != std::nullopt;
    bool do_attach = parser.present("--attach") != std::nullopt;

    if ((!do_spawn && !do_attach) || (do_spawn && do_attach)) {
        fmt::print(stderr, "Must specify one of --spawn or --attach\n");
        return -1;
    }

    fmt::print(stderr, "xnu-single-step-trace-util begin self PID: {:d}\n", getpid());

    pid_t target_pid = -1;
    if (do_attach) {
        const auto name = *parser.present("--attach");
        target_pid      = pid_for_name(name);
        fmt::print("process '{:s}' has pid {:d}\n", name, target_pid);
    }

    assert(!do_spawn && "not implemented");

    task_t target_task;
    const auto tfp_kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    fmt::print("tfp kr: {:#010x} {:s}\n", tfp_kr, mach_error_string(tfp_kr));

    const auto exc_port = create_exception_port(target_task, EXC_MASK_ALL);
    assert(exc_port);

    const auto queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    assert(queue);

    const auto dead_exc_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, exc_port, 1, queue);
    assert(dead_exc_source);
    dispatch_source_set_event_handler(dead_exc_source,
                                      ^{ fmt::print(stderr, "got dead notification\n"); });
    dispatch_resume(dead_exc_source);

    const auto exc_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, exc_port, 0, queue);
    assert(exc_source);
    dispatch_source_set_event_handler(
        exc_source, ^{ dispatch_mig_server(exc_source, EXC_MSG_MAX_SIZE, mach_exc_server); });
    dispatch_resume(exc_source);

    const auto signal_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, queue);
    assert(signal_source);
    dispatch_source_set_event_handler(signal_source, ^{
        fmt::print("got SIGINT, exiting\n");
        exit(0);
    });
    dispatch_resume(signal_source);

    set_single_step_task(target_task, true);

    dispatch_main();

    fmt::print(stderr, "xnu-single-step-trace-util end\n");
    return 0;
}
