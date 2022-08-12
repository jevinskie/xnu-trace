#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include <mach/mach.h>

#include <argparse/argparse.hpp>
#include <fmt/core.h>

static size_t exc_handler_cb(__unused mach_port_t task, __unused mach_port_t thread,
                             exception_type_t type, mach_exception_data_t codes_64) {
    fprintf(stderr, "exc_handler called\n");
    return 4;
}

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

    if (do_attach) {
        const auto name = *parser.present("--attach");
        const auto pid  = pid_for_name(name);
        fmt::print("process '{:s}' has pid {:d}\n", name, pid);
    }

    fmt::print(stderr, "xnu-single-step-trace-util begin\n");

    // mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
    // assert(exc_port);
    // run_exception_handler(exc_port, exc_handler_cb);
    // set_single_step(mach_thread_self(), true);
    // auto crash_ptr = (volatile uint8_t *)0xdeadbeef;
    // *crash_ptr     = 42;

    // fmt::print(stderr, "crash_ptr: {:d}\n", *crash_ptr);

    fmt::print(stderr, "xnu-single-step-trace-util end\n");
    return 0;
}
