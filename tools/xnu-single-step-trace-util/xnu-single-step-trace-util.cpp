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
    parser.add_argument("-s", "--spawn")
        .default_value(false)
        .implicit_value(true)
        .help("spawn process");
    parser.add_argument("-p", "--pipe")
        .default_value(false)
        .implicit_value(true)
        .help("control trace using child pipe");
    parser.add_argument("-a", "--attach")
        .default_value(false)
        .implicit_value(true)
        .help("attach to process");
    parser.add_argument("-A", "--no-aslr")
        .default_value(false)
        .implicit_value(true)
        .help("disable ASLR (spawned processes only)");
    parser.add_argument("spawn-args").remaining().help("spawn executable path and arguments");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    const auto do_spawn     = parser["--spawn"] == true;
    const auto do_attach    = parser["--attach"] == true;
    const auto disable_aslr = parser["--no-aslr"] == true;
    const auto do_pipe      = parser["--pipe"] == true;

    if ((!do_spawn && !do_attach) || (do_spawn && do_attach)) {
        fmt::print(stderr, "Must specify one of --spawn or --attach\n");
        return -1;
    }

    if (do_attach && disable_aslr) {
        fmt::print(stderr, "Can't disable ASLR on attached processes\n");
        return -1;
    }

    if (do_attach && do_pipe) {
        fmt::print(stderr, "Can't control via pipes on attached processes\n");
        return -1;
    }

    fmt::print(stderr, "xnu-single-step-trace-util begin self PID: {:d}\n", getpid());

    std::unique_ptr<XNUTracer> tracer;
    std::optional<int> pipe_read_fd;
    std::optional<int> pipe_write_fd;

    if (do_pipe) {
        int pipe_fds[2];
        assert(!pipe(pipe_fds));
        pipe_read_fd  = pipe_fds[0];
        pipe_write_fd = pipe_fds[1];
    }

    if (do_attach) {
        const auto target_name = *parser.present("--attach");
        tracer                 = std::make_unique<XNUTracer>(target_name);
    } else if (do_spawn) {
        const auto spawn_args = parser.get<std::vector<std::string>>("spawn-args");
        tracer = std::make_unique<XNUTracer>(spawn_args, pipe_write_fd, disable_aslr);
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

    if (do_pipe) {
        const auto pipe_source =
            dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, *pipe_read_fd, 0, queue);
        assert(pipe_source);
        XNUTracer *tracer_raw = tracer.get();
        dispatch_source_set_event_handler(pipe_source, ^{
            uint8_t buf;
            assert(read(*pipe_read_fd, &buf, sizeof(buf)) == sizeof(buf));
            if (buf == 'y') {
                tracer_raw->set_single_step(true);
            } else if (buf == 'n') {
                tracer_raw->set_single_step(false);
            } else {
                assert(!"unhandled");
            }
        });
        dispatch_resume(pipe_source);
        tracer->set_single_step(false);
    }

    const auto breakpoint_exc_source = tracer->breakpoint_exception_port_dispath_source();
    dispatch_resume(breakpoint_exc_source);

    const auto proc_source = tracer->proc_dispath_source();
    dispatch_source_set_event_handler(proc_source, ^{ exit(0); });
    dispatch_resume(proc_source);

    fmt::print("wait begin\n");
    usleep(3'000'000);
    fmt::print("wait end\n");

    tracer->resume();

    dispatch_main();

    fmt::print(stderr, "xnu-single-step-trace-util end\n");
    return 0;
}
