#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>

#include <argparse/argparse.hpp>
#include <fmt/format.h>

void write_drcov_coverage(std::string path, const TraceLog &trace) {
    for (const auto &region : trace.macho_regions().regions()) {
        fmt::print("base: {:#018x} size: {:#010x} path: '{:s}'\n", region.base, region.size,
                   region.path.string());
    }

    for (const auto &thread_trace_pair : trace.parsed_logs()) {
        const auto tid = thread_trace_pair.first;
        const auto log = thread_trace_pair.second;
        fmt::print("tid: {:d} sz: {:d}\n", tid, log.size());
    }

    const auto fh = fopen(path.c_str(), "w");
    assert(fh);

    // for (auto msg_hdr = trace_buf_start; msg_hdr < trace_buf_end;) {
    //     auto msg_sz = sizeof(*msg_hdr);

    //     msg_hdr = (log_msg_hdr *)((uint8_t *)msg_hdr + msg_sz);
    // }

    assert(!fclose(fh));
}

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());
    parser.add_argument("-t", "--trace-file").required().help("input trace file path");
    parser.add_argument("-d", "--drcov-file").help("output drcov coverage file path");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    const auto trace = TraceLog(parser.get("--trace-file"));

    if (const auto drcov_path = parser.present("--drcov-file")) {
        write_drcov_coverage(*drcov_path, trace);
    }

    return 0;
}
