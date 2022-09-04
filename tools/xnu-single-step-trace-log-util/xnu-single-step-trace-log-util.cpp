#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>

#include <argparse/argparse.hpp>
#include <fmt/format.h>

namespace fs = std::filesystem;

void dump_log(const TraceLog &trace, bool symbolicate = false) {
    for (const auto &region : trace.macho_regions().regions()) {
        fmt::print("base: {:#018x} => {:#018x} size: {:#010x} path: '{:s}'\n", region.base,
                   region.base + region.size, region.size, region.path.string());
    }

    if (symbolicate) {
        for (const auto &sym : trace.symbols().syms()) {
            fmt::print("base: {:#018x} sz: {:d} name: {:s} img: {:s}\n", sym.base, sym.size,
                       sym.name, fs::path{sym.path}.filename().string());
        }
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

void dump_calls_from(const TraceLog &trace, const std::string &calling_image) {
    std::vector<std::vector<bb_t>> tbbs;
    for (const auto &ttp : trace.parsed_logs()) {
        tbbs.emplace_back(extract_bbs_from_pc_trace(extract_pcs_from_trace(ttp.second)));
    }
    const auto macho_regions        = trace.macho_regions();
    const auto syms                 = trace.symbols();
    const image_info *last_img_info = nullptr;
    for (const auto &bbs : tbbs) {
        for (const auto &bb : bbs) {
            const auto img_info = macho_regions.lookup(bb.pc);
            const auto *sym     = syms.lookup(bb.pc);
            if (last_img_info && sym && sym->name == "_fopen") {
                fmt::print("found fopen last path: {:s} off: {:#x}\n", last_img_info->path.string(),
                           bb.pc - sym->base);
            }
            if (last_img_info &&
                fs::path{last_img_info->path}.filename().string() == calling_image && sym &&
                sym->base == bb.pc) {
                fmt::print("{:s} calls {:s} in {:s}\n", calling_image, sym->name,
                           fs::path{sym->path}.filename().string());
            }
            last_img_info = &img_info;
        }
    }
}

void write_lighthouse_coverage(std::string path, const TraceLog &trace, bool symbolicate = false) {
    std::vector<std::vector<bb_t>> tbbs;
    for (const auto &ttp : trace.parsed_logs()) {
        tbbs.emplace_back(extract_bbs_from_pc_trace(extract_pcs_from_trace(ttp.second)));
    }

    const auto fh = fopen(path.c_str(), "w");
    assert(fh);

    const auto macho_regions = trace.macho_regions();
    const auto syms          = trace.symbols();
    for (const auto &bbs : tbbs) {
        for (const auto &bb : bbs) {
            const auto img_info = macho_regions.lookup(bb.pc);
            if (!symbolicate) {
                fmt::print(fh, "{:s}+{:x}\n", img_info.path.filename().string(),
                           bb.pc - img_info.base);
            } else {
                const auto *sym = syms.lookup(bb.pc);
                if (sym) {
                    fmt::print(fh, "{:s}+{:x} {:s}+{:x}\n", img_info.path.filename().string(),
                               bb.pc - img_info.base, sym->name, bb.pc - sym->base);
                } else {
                    fmt::print(fh, "{:s}+{:x}\n", img_info.path.filename().string(),
                               bb.pc - img_info.base);
                }
            }
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
    parser.add_argument("-c", "--calls-from").help("find calls from given image");
    parser.add_argument("-S", "--symbolicate")
        .default_value(false)
        .implicit_value(true)
        .help("print symbol information");
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

    const auto symbolicate = parser["--symbolicate"] == true;

    const auto trace = TraceLog(parser.get("--trace-file"));

    if (const auto path = parser.present("--drcov-file")) {
        write_drcov_coverage(*path, trace);
    }

    if (const auto path = parser.present("--lighthouse-file")) {
        write_lighthouse_coverage(*path, trace, symbolicate);
    }

    if (parser.get<bool>("--dump")) {
        dump_log(trace, symbolicate);
    }

    if (const auto calling_image = parser.present("--calls-from")) {
        dump_calls_from(trace, *calling_image);
    }

    return 0;
}
