#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <set>

#include <argparse/argparse.hpp>
#include <fmt/format.h>

namespace fs = std::filesystem;

void dump_log(const TraceLog &trace, bool symbolicate = false) {
    trace.macho_regions().dump();

    if (symbolicate) {
        for (const auto &sym : trace.symbols().syms()) {
            fmt::print("base: {:#018x} sz: {:d} name: {:s} img: {:s}\n", sym.base, sym.size,
                       sym.name, fs::path{sym.path}.filename().string());
        }
    }

#if 0
    for (const auto &[tid, log] : trace.parsed_logs()) {
        fmt::print("tid: {:d} sz: {:d}\n", tid, log.size());
        const auto bbs = extract_bbs_from_pc_trace(extract_pcs_from_trace(log));
        for (const auto &bb : bbs) {
            fmt::print("BB: {:#018x} [{:d}]\n", bb.pc, bb.sz);
        }
    }
#endif
}

void dump_histogram(const TraceLog &trace, int max_num) {
    ARM64InstrHistogram hist;
    const auto regions = trace.macho_regions();
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto pc : extract_pcs_from_trace(log)) {
            hist.add(regions.lookup_inst(pc));
        }
    }
    hist.print(max_num);
}

void dump_calls_from(const TraceLog &trace, const std::string &calling_image) {
    std::vector<std::vector<bb_t>> tbbs;
    for (const auto &ttp : trace.parsed_logs()) {
        tbbs.emplace_back(extract_bbs_from_pc_trace(extract_pcs_from_trace(ttp.second)));
    }
    const auto macho_regions        = trace.macho_regions();
    const auto syms                 = trace.symbols();
    const auto &target_img_info     = macho_regions.lookup(calling_image);
    const image_info *last_img_info = nullptr;
    const bb_t *last_bb             = nullptr;
    std::map<sym_info, std::set<uint64_t>> called_syms;
    for (const auto &bbs : tbbs) {
        for (const auto &bb : bbs) {
            const auto &img_info = macho_regions.lookup(bb.pc);
            const auto *sym      = syms.lookup(bb.pc);
            if (last_img_info && last_img_info == &target_img_info && sym && sym->base == bb.pc) {
                const auto calling_pc        = last_bb->pc + last_bb->sz - 4;
                const auto calling_pc_unslid = calling_pc - last_img_info->slide;
                auto [callers, found] =
                    called_syms.emplace(std::make_pair(*sym, std::set<uint64_t>{}));
                callers->second.emplace(calling_pc_unslid);
            }
            last_img_info = &img_info;
            last_bb       = &bb;
        }
    }
    for (const auto &sym_it : called_syms) {
        std::string sym_name   = sym_it.first.name;
        const auto &sym        = sym_it.first;
        const auto &caller_pcs = sym_it.second;
        if (sym_name == "n/a") {
            sym_name = fmt::format("{:#018x}", sym.base);
        }
        fmt::print("{:s} calls '{:s}' in {:s} from: {:#018x}\n", calling_image, sym_name,
                   sym.path.filename().string(), fmt::join(caller_pcs, ", "));
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
    parser.add_argument("-H", "--histogram")
        .default_value(false)
        .implicit_value(true)
        .help("dump instruction histogram to console");
    parser.add_argument("-n", "--max-histogram-insts")
        .scan<'i', int>()
        .default_value(-1)
        .help("print top N most frequent instructions");

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

    if (parser.get<bool>("--histogram")) {
        dump_histogram(trace, parser.get<int>("--max-histogram-insts"));
    }

    if (const auto calling_image = parser.present("--calls-from")) {
        dump_calls_from(trace, *calling_image);
    }

    return 0;
}
