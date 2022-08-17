#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <argparse/argparse.hpp>
#include <fmt/core.h>

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());
    parser.add_argument("--spawn").help("spawn process");
    parser.add_argument("--attach").help("attach to process");
    parser.add_argument("--no-aslr").help("disable ASLR (spawned processes only)");
    parser.add_argument("spawn-args").remaining().help("spawn executable path and arguments");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    bool do_spawn     = parser.present("--spawn") != std::nullopt;
    bool do_attach    = parser.present("--attach") != std::nullopt;
    bool disable_aslr = parser.present("--no-aslr") != std::nullopt;

    if ((!do_spawn && !do_attach) || (do_spawn && do_attach)) {
        fmt::print(stderr, "Must specify one of --spawn or --attach\n");
        return -1;
    }

    if (do_attach && disable_aslr) {
        fmt::print(stderr, "Can't disable ASLR on attached processes\n");
        return -1;
    }

    fmt::print(stderr, "xnu-single-step-trace-util begin self PID: {:d}\n", getpid());

    std::unique_ptr<XNUTracer> tracer;

    if (do_attach) {
        const auto target_name = *parser.present("--attach");
        tracer                 = std::make_unique<XNUTracer>(target_name);
    } else if (do_spawn) {
        const auto spawn_args = parser.get<std::vector<std::string>>("spawn-args");
        tracer                = std::make_unique<XNUTracer>(spawn_args, disable_aslr);
    } else {
        assert(!"nothing to do");
    }

    const auto queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    assert(queue);

    const auto signal_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, queue);
    assert(signal_source);
    dispatch_source_set_event_handler(signal_source, ^{ exit(0); });
    dispatch_resume(signal_source);

    const auto breakpoint_exc_source = tracer->breakpoint_exception_port_dispath_source();
    dispatch_resume(breakpoint_exc_source);

    dispatch_main();

    fmt::print(stderr, "xnu-single-step-trace-util end\n");
    return 0;
}
