#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>

#include <argparse/argparse.hpp>
#include <fmt/format.h>

std::vector<bb_t> extract_bbs_from_pc_trace(const std::vector<uint64_t> &pcs) {
    std::vector<bb_t> bbs;

    uint64_t bb_start = pcs[0];
    uint64_t last_pc  = pcs[0] - 4;
    for (const auto pc : pcs) {
        if (last_pc + 4 != pc) {
            bbs.emplace_back(bb_t{.pc = bb_start, .sz = (uint32_t)(last_pc + 4 - bb_start)});
            bb_start = pc;
        }
        last_pc = pc;
    }
    if (bb_start != last_pc) {
        bbs.emplace_back(bb_t{.pc = bb_start, .sz = (uint32_t)(last_pc + 4 - bb_start)});
    }
    return bbs;
}

std::vector<uint64_t> extract_pcs_from_trace(const std::vector<log_msg_hdr> &msgs) {
    std::vector<uint64_t> pcs;
    for (const auto &msg : msgs) {
        pcs.emplace_back(msg.pc);
    }
    return pcs;
}

void dump_log(const TraceLog &trace) {
    for (const auto &region : trace.macho_regions().regions()) {
        fmt::print("base: {:#018x} size: {:#010x} path: '{:s}'\n", region.base, region.size,
                   region.path.string());
    }

    for (const auto &thread_trace_pair : trace.parsed_logs()) {
        const auto tid = thread_trace_pair.first;
        const auto log = thread_trace_pair.second;
        fmt::print("tid: {:d} sz: {:d}\n", tid, log.size());
        const auto bbs = extract_bbs_from_pc_trace(extract_pcs_from_trace(log));
        for (const auto &bb : bbs) {
            fmt::print("BB: {:#018x} [{:d}]\n", bb.pc, bb.sz);
        }
    }
}

void write_lighthouse_coverage(std::string path, const TraceLog &trace) {
    std::vector<std::vector<bb_t>> tbbs;
    for (const auto &ttp : trace.parsed_logs()) {
        tbbs.emplace_back(extract_bbs_from_pc_trace(extract_pcs_from_trace(ttp.second)));
    }

    const auto fh = fopen(path.c_str(), "w");
    assert(fh);

    const auto macho_regions = trace.macho_regions();
    for (const auto &bbs : tbbs) {
        for (const auto &bb : bbs) {
            const auto [img_info, idx] = macho_regions.lookup_idx(bb.pc);
            fmt::print(fh, "{:s}+{:x}\n", img_info.path.filename().string(), bb.pc - img_info.base);
        }
    }

    assert(!fclose(fh));
}

void write_drcov_coverage(std::string path, const TraceLog &trace) {
    const auto regions = trace.macho_regions().regions();

    std::vector<std::vector<bb_t>> tbbs;
    for (const auto &ttp : trace.parsed_logs()) {
        tbbs.emplace_back(extract_bbs_from_pc_trace(extract_pcs_from_trace(ttp.second)));
    }

    const auto fh = fopen(path.c_str(), "w");
    assert(fh);

    fmt::print(fh,
               R"scr(DRCOV VERSION: 2
DRCOV FLAVOR: xnutrace
Module Table: version 2, count {:d}
Columns: id, base, end, entry, checksum, timestamp, path
)scr",
               regions.size());

    int i = 0;
    for (const auto &bin : regions) {
        fmt::print(fh, "{:3d}, {:#018x}, {:#018x}, 0, 0, 0, {:s}\n", i, bin.base,
                   bin.base + bin.size, bin.path.string());
        ++i;
    }

    size_t num_bbs = 0;
    for (const auto &bbs : tbbs) {
        num_bbs += bbs.size();
    }

    fmt::print(fh, "BB Table: {:d} bbs\n", num_bbs);

    const auto macho_regions = trace.macho_regions();
    for (const auto &bbs : tbbs) {
        for (const auto &bb : bbs) {
            const auto [img_info, idx] = macho_regions.lookup_idx(bb.pc);
            drcov_bb_t dbb{.mod_off = (uint32_t)(bb.pc - img_info.base),
                           .sz      = (uint16_t)bb.sz,
                           .mod_id  = (uint16_t)idx};
            assert(fwrite(&dbb, sizeof(dbb), 1, fh) == 1);
        }
    }

    assert(!fclose(fh));
}

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());
    parser.add_argument("-t", "--trace-file").required().help("input trace file path");
    parser.add_argument("-d", "--drcov-file").help("output drcov coverage file path");
    parser.add_argument("-l", "--lighthouse-file").help("output lighthouse coverage file path");
    parser.add_argument("-D", "--dump")
        .default_value(false)
        .implicit_value(true)
        .help("dump trace log to console");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    const auto trace = TraceLog(parser.get("--trace-file"));

    if (const auto path = parser.present("--drcov-file")) {
        write_drcov_coverage(*path, trace);
    }

    if (const auto path = parser.present("--lighthouse-file")) {
        write_lighthouse_coverage(*path, trace);
    }

    if (parser.get<bool>("--dump")) {
        dump_log(trace);
    }

    return 0;
}
