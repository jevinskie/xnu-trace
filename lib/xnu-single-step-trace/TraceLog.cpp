#include "common.h"

#include <set>

std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs) {
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

std::vector<uint64_t> extract_pcs_from_trace(const std::span<const log_msg_hdr> &msgs) {
    std::vector<uint64_t> pcs;
    for (const auto &msg : msgs) {
        pcs.emplace_back(msg.pc);
    }
    return pcs;
}

TraceLog::TraceLog() {
    // nothing to do
}

TraceLog::TraceLog(const std::string &log_path) {
    fs::path path{log_path};
    CompressedFile<log_meta_hdr> meta_fh{path / "meta.bin", true, log_meta_hdr_magic};
    const auto meta_buf = meta_fh.read();
    const auto meta_hdr = meta_fh.header();

    auto region_ptr = (log_region *)meta_buf.data();
    m_macho_regions = std::make_unique<MachORegions>(region_ptr, meta_hdr.num_regions);
    for (uint64_t i = 0; i < meta_hdr.num_regions; ++i) {
        region_ptr =
            (log_region *)((uint8_t *)region_ptr + sizeof(*region_ptr) + region_ptr->path_len);
    }

    auto syms_ptr = (log_sym *)region_ptr;
    m_symbols     = std::make_unique<Symbols>(syms_ptr, meta_hdr.num_syms);
    for (uint64_t i = 0; i < meta_hdr.num_syms; ++i) {
        syms_ptr = (log_sym *)((uint8_t *)syms_ptr + sizeof(*syms_ptr) + syms_ptr->name_len +
                               syms_ptr->path_len);
    }

    for (const auto &dirent : std::filesystem::directory_iterator{path}) {
        if (dirent.path().filename() == "meta.bin") {
            continue;
        }
        assert(dirent.path().filename().string().starts_with("trace-"));

        CompressedFile<log_thread_hdr> thread_fh{dirent.path(), true, log_thread_hdr_magic};
        const auto thread_buf = thread_fh.read();
        const auto thread_hdr = thread_fh.header();

        std::vector<log_msg_hdr> thread_log;
        auto inst_hdr           = (log_msg_hdr *)thread_buf.data();
        const auto inst_hdr_end = (log_msg_hdr *)(thread_buf.data() + thread_buf.size());
        while (inst_hdr < inst_hdr_end) {
            thread_log.emplace_back(*inst_hdr);
            inst_hdr = inst_hdr + 1;
        }
        m_parsed_logs.emplace(std::make_pair(thread_hdr.thread_id, thread_log));
    }
}

uint64_t TraceLog::num_inst() const {
    return m_num_inst;
}

size_t TraceLog::num_bytes() const {
    size_t sz = 0;
    for (const auto &thread_log : m_log_bufs) {
        sz += thread_log.second.size();
    }
    return sz;
}

const MachORegions &TraceLog::macho_regions() const {
    assert(m_macho_regions);
    return *m_macho_regions;
}

const Symbols &TraceLog::symbols() const {
    assert(m_symbols);
    return *m_symbols;
}

const std::map<uint32_t, std::vector<log_msg_hdr>> &TraceLog::parsed_logs() const {
    return m_parsed_logs;
}

__attribute__((always_inline)) void TraceLog::log(thread_t thread, uint64_t pc) {
    const auto msg_hdr = log_msg_hdr{.pc = pc};
    std::copy((uint8_t *)&msg_hdr, (uint8_t *)&msg_hdr + sizeof(msg_hdr),
              std::back_inserter(m_log_bufs[thread]));
    ++m_num_inst;
}

void TraceLog::write_to_dir(const std::string &dir_path, const MachORegions &macho_regions,
                            int compression_level, const Symbols *symbols) {
    fs::path path{dir_path};

    std::set<uint64_t> pcs;
    for (const auto &thread_buf_pair : m_log_bufs) {
        const auto buf = thread_buf_pair.second;
        for (const auto pc : extract_pcs_from_trace(
                 {(log_msg_hdr *)buf.data(), buf.size() / sizeof(log_msg_hdr)})) {
            pcs.emplace(pc);
        }
    }
    interval_tree_t<uint64_t> pc_intervals;
    for (const auto pc : pcs) {
        pc_intervals.insert_overlap({pc, pc + 4});
    }

    std::vector<sym_info> syms;
    if (symbols) {
        const auto all_syms = symbols->syms();
        syms                = get_symbols_in_intervals(all_syms, pc_intervals);
    }

    const log_meta_hdr meta_hdr_buf{.num_regions = macho_regions.regions().size(),
                                    .num_syms    = syms.size()};

    fs::remove_all(path);
    fs::create_directory(path);
    CompressedFile<log_meta_hdr> meta_fh{path / "meta.bin", false, log_meta_hdr_magic,
                                         &meta_hdr_buf, 0};

    for (const auto &region : macho_regions.regions()) {
        log_region region_buf{.base     = region.base,
                              .size     = region.size,
                              .slide    = region.slide,
                              .path_len = region.path.string().size()};
        memcpy(region_buf.uuid, region.uuid, sizeof(region_buf.uuid));
        meta_fh.write(region_buf);
        meta_fh.write(region.path.c_str(), region.path.string().size());
    }

    for (const auto &sym : syms) {
        log_sym sym_buf{.base     = sym.base,
                        .size     = sym.size,
                        .name_len = sym.name.size(),
                        .path_len = sym.path.string().size()};
        meta_fh.write(sym_buf);
        meta_fh.write(sym.name.c_str(), sym.name.size());
        meta_fh.write(sym.path.c_str(), sym.path.string().size());
    }

    for (const auto &thread_buf_pair : m_log_bufs) {
        const auto tid = thread_buf_pair.first;
        const auto buf = thread_buf_pair.second;
        const log_thread_hdr thread_hdr{.thread_id = tid};
        CompressedFile<log_thread_hdr> thread_fh{path / fmt::format("thread-{:d}.bin", tid), false,
                                                 log_thread_hdr_magic, &thread_hdr,
                                                 compression_level};
        thread_fh.write(thread_hdr);
        thread_fh.write(buf);
    }
}
