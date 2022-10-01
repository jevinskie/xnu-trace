#include "xnu-trace/TraceLog.h"
#include "common-internal.h"

#include "xnu-trace/Signpost.h"
#include "xnu-trace/ThreadPool.h"
#include "xnu-trace/mach.h"

#include <bit>

#include <absl/container/flat_hash_set.h>
#include <arm_neon.h>
#include <interval-tree/interval_tree.hpp>

using namespace lib_interval_tree;

std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs) {
    std::vector<bb_t> bbs;

    // TODO: vectorize: vector if pcs - last_pcs != {4, 8, 12, 16} then scalar code else next
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

std::vector<bb_t> extract_bbs_from_trace(const log_thread_buf &thread_buf) {
    std::vector<bb_t> bbs;
    (void)thread_buf;
    assert(!"not implemented");
}

std::vector<uint64_t> extract_pcs_from_trace(const log_thread_buf &thread_buf) {
    std::vector<uint64_t> pcs;
    pcs.reserve(thread_buf.num_inst());
    for (auto i = thread_buf.pcs_begin(), e = thread_buf.pcs_end(); i != e; ++i) {
        pcs.emplace_back(i.pc());
    }
    return pcs;
}

TraceLog::TraceLog(const std::string &log_dir_path, int compression_level, bool stream)
    : m_log_dir_path{log_dir_path}, m_compression_level{compression_level}, m_stream{stream} {
    fs::create_directory(m_log_dir_path);
    for (const auto &dirent : std::filesystem::directory_iterator{m_log_dir_path}) {
        if (!dirent.path().filename().string().starts_with("macho-region-")) {
            fs::remove(dirent.path());
        }
    }
}

TraceLog::TraceLog(const std::string &log_dir_path) : m_log_dir_path{log_dir_path} {
    Signpost meta_sp("TraceLog", "meta.bin read");
    meta_sp.start();
    CompressedFile<log_meta_hdr> meta_fh{m_log_dir_path / "meta.bin", true};
    const auto meta_buf = meta_fh.read();
    const auto meta_hdr = meta_fh.header();
    meta_sp.end();

    Signpost regions_sp("TraceLog", "regions read");
    regions_sp.start();
    std::vector<fs::path> regions_paths;
    regions_paths.reserve(meta_hdr.num_regions);
    for (const auto &dirent : std::filesystem::directory_iterator{log_dir_path}) {
        if (!dirent.path().filename().string().starts_with("macho-region-")) {
            continue;
        }
        regions_paths.emplace_back(dirent.path());
    }
    assert(regions_paths.size() == meta_hdr.num_regions);

    std::vector<std::pair<sha256_t, std::vector<uint8_t>>> regions_bytes_vec(meta_hdr.num_regions);
    for (size_t i = 0; i < meta_hdr.num_regions; ++i) {
        xnutrace_pool.push_task([&, i] {
            const auto path = regions_paths[i];
            Signpost region_sp("TraceLogRegions",
                               fmt::format("{:s} read", path.filename().string()));
            region_sp.start();
            CompressedFile<log_macho_region_hdr> region_fh{path, true};
            sha256_t digest;
            memcpy(digest.data(), region_fh.header().digest_sha256, digest.size());
            regions_bytes_vec[i] = {digest, region_fh.read()};
            region_sp.end();
        });
    }
    xnutrace_pool.wait_for_tasks();

    std::map<sha256_t, std::vector<uint8_t>> regions_bytes;
    for (const auto &[digest, bytes] : regions_bytes_vec) {
        regions_bytes.emplace(digest, std::move(bytes));
    }

    auto region_ptr = (log_region *)meta_buf.data();
    m_macho_regions =
        std::make_unique<MachORegions>(region_ptr, meta_hdr.num_regions, regions_bytes);
    for (uint64_t i = 0; i < meta_hdr.num_regions; ++i) {
        region_ptr =
            (log_region *)((uint8_t *)region_ptr + sizeof(*region_ptr) + region_ptr->path_len);
    }
    regions_sp.end();

    Signpost syms_sp("TraceLog", "symbols read");
    syms_sp.start();
    auto syms_ptr = (log_sym *)region_ptr;
    m_symbols     = std::make_unique<Symbols>(syms_ptr, meta_hdr.num_syms);
    for (uint64_t i = 0; i < meta_hdr.num_syms; ++i) {
        syms_ptr = (log_sym *)((uint8_t *)syms_ptr + sizeof(*syms_ptr) + syms_ptr->name_len +
                               syms_ptr->path_len);
    }
    syms_sp.end();

    Signpost threads_sp("TraceLog", "threads read & parse");
    threads_sp.start();

    std::vector<fs::path> thread_paths;
    for (const auto &dirent : std::filesystem::directory_iterator{log_dir_path}) {
        const auto fn = dirent.path().filename();
        if (fn == "meta.bin" || fn.string().starts_with("macho-region-")) {
            continue;
        }
        assert(fn.string().starts_with("thread-"));
        thread_paths.emplace_back(dirent.path());
    }

    std::vector<std::pair<uint32_t, log_thread_buf>> parsed_logs_vec(thread_paths.size());
    for (size_t i = 0; i < thread_paths.size(); ++i) {
        xnutrace_pool.push_task([&, i] {
            const auto path = thread_paths[i];
            Signpost thread_read_sp("TraceLogThreads",
                                    fmt::format("{:s} read", path.filename().string()));
            thread_read_sp.start();
            CompressedFile<log_thread_hdr> thread_fh{path, true};
            auto thread_buf       = thread_fh.read();
            const auto thread_hdr = thread_fh.header();
            thread_read_sp.end();

            Signpost thread_parse_sp("TraceLogThreads",
                                     fmt::format("{:s} parse", path.filename().string()));
            thread_parse_sp.start();
            parsed_logs_vec[i] = std::make_pair(
                thread_hdr.thread_id, log_thread_buf(std::move(thread_buf), thread_hdr.num_inst));
            thread_parse_sp.end();
        });
    }
    xnutrace_pool.wait_for_tasks();

    for (auto &[thread_id, log] : parsed_logs_vec) {
        m_num_inst += log.num_inst();
        m_parsed_logs.emplace(thread_id, std::move(log));
    }
    threads_sp.end();
}

uint64_t TraceLog::num_inst() const {
    return m_num_inst;
}

size_t TraceLog::num_bytes() const {
    size_t sz = 0;
    for (const auto &[tid, ctx] : m_thread_ctxs) {
        if (!m_stream) {
            sz += ctx.log_buf.size();
        } else {
            sz += ctx.log_stream->decompressed_size();
        }
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

const std::map<uint32_t, log_thread_buf> &TraceLog::parsed_logs() const {
    return m_parsed_logs;
}

static uint64x2_t interleave_uint64x2_with_zeros_16bit(uint64x2_t input) {
    uint64x2_t word = input;
    word            = (word ^ (word << 8)) & 0x00ff'00ff;
    word            = (word ^ (word << 4)) & 0x0f0f'0f0f;
    word            = (word ^ (word << 2)) & 0x3333'3333;
    word            = (word ^ (word << 1)) & 0x5555'5555;
    return word;
}

void TraceLog::thread_ctx::write_log_msg(const log_arm64_cpu_context *ctx) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
    uint8_t __attribute__((uninitialized, aligned(16))) msg_buf[log_msg::size_full_ctx];
#pragma clang diagnostic pop

    if (sz_since_last_sync >= sync_every) {
        write_sync();
    }

    auto *msg_hdr        = (log_msg *)msg_buf;
    uint8_t *buf_ptr     = msg_buf + sizeof(log_msg);
    uint32_t gpr_changed = 0;
    uint32_t vec_changed = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Waddress-of-packed-member"
    const auto last_pc_sp = *(uint64x2_t *)&last_cpu_ctx.pc;
    const auto pc_sp      = *(uint64x2_t *)&ctx->pc;
#pragma clang diagnostic pop
    const auto pc_sp_diff = pc_sp - last_pc_sp;
    if (pc_sp_diff[0] != 4) {
        gpr_changed          = rpc_set_pc_branched(gpr_changed);
        *(uint64_t *)buf_ptr = pc_sp[0];
        buf_ptr += sizeof(uint64_t);
    }
    if (pc_sp_diff[1] != 0) {
        gpr_changed          = rpc_set_sp_changed(gpr_changed);
        *(uint64_t *)buf_ptr = pc_sp[1];
        buf_ptr += sizeof(uint64_t);
    }

    const auto last_x2  = (uint64x2_t *)&last_cpu_ctx.x[0];
    const auto x2       = (uint64x2_t *)&ctx->x[0];
    uint64x2_t vx2_diff = {};
    for (int i = 0; i < 32 / 2; ++i) {
        auto diff_mask = x2[i] != last_x2[i];
        diff_mask &= 1;
        diff_mask <<= i;
        vx2_diff |= diff_mask;
    }
    vx2_diff        = interleave_uint64x2_with_zeros_16bit(vx2_diff);
    uint32_t x_diff = vx2_diff[0] | (vx2_diff[1] << 1);
    x_diff &= ~(1 << 31); // 32nd slot isn't used (x[29], fp, lr) and comes from v[] so ignore

    const auto last_v = (uint64x2_t *)&last_cpu_ctx.v[0];
    const auto v      = (uint64x2_t *)&ctx->v[0];
    uint32_t v_diff   = 0;
    for (int i = 0; i < 32; ++i) {
        auto vneq = v[i] != last_v[i];
        v_diff |= ((vneq[0] | vneq[1]) & 1) << i;
    }

    const auto num_gpr_changed = std::popcount(x_diff);
    assert(XNUTRACE_LIKELY(num_gpr_changed <= rpc_num_changed_max));
    gpr_changed         = rpc_set_num_changed(gpr_changed, num_gpr_changed);
    uint8_t gpr_idx     = 0;
    const auto *gpr_ptr = (uint64_t *)&ctx->x[0];
    for (int i = 0; i < 31; ++i) {
        if ((x_diff >> i) & 1) {
            *(uint64_t *)buf_ptr = gpr_ptr[i];
            buf_ptr += sizeof(uint64_t);
            gpr_changed = rpc_set_reg_idx(gpr_changed, gpr_idx++, i);
        }
    }

    const auto num_vec_changed = std::popcount(v_diff);
    assert(XNUTRACE_LIKELY(num_vec_changed <= rpc_num_changed_max));
    vec_changed     = rpc_set_num_changed(vec_changed, num_vec_changed);
    uint8_t vec_idx = 0;
    for (int i = 0; i < 32; ++i) {
        if ((v_diff >> i) & 1) {
            *(uint128_t *)buf_ptr = *(uint128_t *)&ctx->v[i];
            buf_ptr += sizeof(uint128_t);
            vec_changed = rpc_set_reg_idx(vec_changed, vec_idx++, i);
        }
    }

    msg_hdr->gpr_changed = gpr_changed;
    msg_hdr->vec_changed = vec_changed;

    const auto msg_sz = buf_ptr - msg_buf;
    if (!log_stream) {
        std::copy(msg_buf, buf_ptr, std::back_inserter(log_buf));
    } else {
        log_stream->write(msg_buf, msg_sz);
    }
    sz_since_last_sync += msg_sz;

    memcpy(&last_cpu_ctx, ctx, sizeof(last_cpu_ctx));
    ++num_inst;
}

void TraceLog::thread_ctx::write_log_msg(uint64_t pc) {
    uint8_t __attribute__((uninitialized, aligned(16))) msg_buf[sizeof(log_msg) + sizeof(uint64_t)];
    auto *msg_hdr        = (log_msg *)msg_buf;
    uint8_t *buf_ptr     = msg_buf + sizeof(log_msg);
    uint32_t gpr_changed = 0;

    if (last_cpu_ctx.pc + 4 != pc) {
        gpr_changed          = rpc_set_pc_branched(gpr_changed);
        *(uint64_t *)buf_ptr = pc;
        buf_ptr += sizeof(uint64_t);
    }
    msg_hdr->gpr_changed = gpr_changed;
    msg_hdr->vec_changed = 0;
    const auto msg_sz    = buf_ptr - (uint8_t *)msg_hdr;
    if (!log_stream) {
        std::copy(msg_buf, buf_ptr, std::back_inserter(log_buf));
    } else {
        log_stream->write(msg_buf, msg_sz);
    }
    last_cpu_ctx.pc = pc;
    ++num_inst;
}

void TraceLog::thread_ctx::write_sync() {
    if (!log_stream) {
        std::copy((uint8_t *)&log_msg::sync_frame_buf,
                  (uint8_t *)&log_msg::sync_frame_buf + sizeof(log_msg::sync_frame_buf),
                  std::back_inserter(log_buf));
        std::copy((uint8_t *)&last_cpu_ctx, (uint8_t *)&last_cpu_ctx + sizeof(last_cpu_ctx),
                  std::back_inserter(log_buf));
    } else {
        log_stream->write((uint8_t *)&log_msg::sync_frame_buf, sizeof(log_msg::sync_frame_buf));
        log_stream->write((uint8_t *)&last_cpu_ctx, sizeof(last_cpu_ctx));
    }
    sz_since_last_sync = 0;
}

void TraceLog::log(thread_t thread, const log_arm64_cpu_context *context) {
    thread_ctx *tctx;
    if (!m_thread_ctxs.contains(thread)) {
        std::unique_ptr<CompressedFile<log_thread_hdr>> log_stream;
        if (m_stream) {
            log_thread_hdr thread_hdr{.thread_id = thread};
            log_stream = std::make_unique<CompressedFile<log_thread_hdr>>(
                m_log_dir_path / fmt::format("thread-{:d}.bin", thread), false, &thread_hdr,
                m_compression_level);
        }
        auto [new_thread_ctx, added] =
            m_thread_ctxs.try_emplace(thread, thread_ctx{.log_stream = std::move(log_stream)});
        assert(added);
        memcpy(&new_thread_ctx.last_cpu_ctx, context, sizeof(new_thread_ctx.last_cpu_ctx));
        tctx = &new_thread_ctx;
    } else {
        tctx = &m_thread_ctxs[thread];
    }
    tctx->write_log_msg(context);
    ++m_num_inst;
}

void TraceLog::log(thread_t thread, uint64_t pc) {
    thread_ctx *tctx;
    if (!m_thread_ctxs.contains(thread)) {
        std::unique_ptr<CompressedFile<log_thread_hdr>> log_stream;
        if (m_stream) {
            log_thread_hdr thread_hdr{.thread_id = thread};
            log_stream = std::make_unique<CompressedFile<log_thread_hdr>>(
                m_log_dir_path / fmt::format("thread-{:d}.bin", thread), false, &thread_hdr,
                m_compression_level);
        }
        auto [new_thread_ctx, added] =
            m_thread_ctxs.try_emplace(thread, thread_ctx{.log_stream = std::move(log_stream)});
        assert(added);
        const auto context = log_arm64_cpu_context{.pc = pc};
        memcpy(&new_thread_ctx.last_cpu_ctx, &context, sizeof(new_thread_ctx.last_cpu_ctx));
        tctx = &new_thread_ctx;
    } else {
        tctx = &m_thread_ctxs[thread];
    }
    tctx->write_log_msg(pc);
    ++m_num_inst;
}

void TraceLog::write(const MachORegions &macho_regions, const Symbols *symbols) {
    absl::flat_hash_map<uint32_t, log_thread_buf> thread_bufs;
    if (!m_stream) {
        for (auto &[tid, ctx] : m_thread_ctxs) {
            const auto tbuf = log_thread_buf(std::move(ctx.log_buf), ctx.num_inst);
            thread_bufs.try_emplace(tid, tbuf);
        }
    }

    interval_tree_t<uint64_t> pc_intervals;
    if (!m_stream) {
        absl::flat_hash_set<uint64_t> pcs;
        for (auto &[tid, tbuf] : thread_bufs) {
            for (const auto pc : extract_pcs_from_trace(tbuf)) {
                pcs.emplace(pc);
            }
        }
        for (const auto pc : pcs) {
            pc_intervals.insert_overlap({pc, pc + 4});
        }
    } else {
        // FIXME: streams just add all symbols
        for (const auto &region : macho_regions.regions()) {
            pc_intervals.insert_overlap({region.base, region.base + region.size});
        }
    }

    std::vector<sym_info> syms;
    if (symbols) {
        const auto all_syms = symbols->syms();
        syms                = get_symbols_in_intervals(all_syms, pc_intervals);
    }

    const log_meta_hdr meta_hdr_buf{.num_regions = macho_regions.regions().size(),
                                    .num_syms    = syms.size()};
    CompressedFile<log_meta_hdr> meta_fh{m_log_dir_path / "meta.bin", false, &meta_hdr_buf, 0};

    for (const auto &region : macho_regions.regions()) {
        log_region region_buf{.base     = region.base,
                              .size     = region.size,
                              .slide    = region.slide,
                              .path_len = region.path.string().size(),
                              .is_jit   = region.is_jit};
        memcpy(region_buf.uuid, region.uuid, sizeof(region_buf.uuid));
        memcpy(region_buf.digest_sha256, region.digest.data(), sizeof(region_buf.digest_sha256));
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

    // find macho-region-*.bin that are unchanged
    std::set<fs::path> reused_macho_regions;
    for (const auto &region : macho_regions.regions()) {
        const auto old_region = m_log_dir_path / region.log_path();
        if (!fs::exists(old_region)) {
            continue;
        }
        CompressedFile<log_macho_region_hdr> old_region_fh{old_region, true};
        if (!memcmp(old_region_fh.header().digest_sha256, region.digest.data(),
                    region.digest.size())) {
            reused_macho_regions.emplace(old_region);
        }
    }

    // remove all macho-regions-*.bin that aren't reused
    for (const auto &dirent : std::filesystem::directory_iterator{m_log_dir_path}) {
        if (dirent.path().filename().string().starts_with("macho-region-") &&
            !reused_macho_regions.contains(dirent.path())) {
            fs::remove(dirent.path());
        }
    }

    for (const auto &region : macho_regions.regions()) {
        const auto region_path = m_log_dir_path / region.log_path();
        if (reused_macho_regions.contains(region_path)) {
            continue;
        }
        log_macho_region_hdr macho_region_hdr_buf{};
        memcpy(macho_region_hdr_buf.digest_sha256, region.digest.data(), region.digest.size());
        CompressedFile<log_macho_region_hdr> macho_region_fh{region_path, false,
                                                             &macho_region_hdr_buf, 1};
        macho_region_fh.write(region.bytes);
    }

    for (const auto &[tid, ctx] : m_thread_ctxs) {
        if (!m_stream) {
            const log_thread_hdr thread_hdr{.thread_id = tid, .num_inst = ctx.num_inst};
            CompressedFile<log_thread_hdr> thread_fh{
                m_log_dir_path / fmt::format("thread-{:d}.bin", tid), false, /* read */
                &thread_hdr, m_compression_level, true /* verbose */};
            thread_fh.write(ctx.log_buf);
        } else {
            ctx.log_stream->header().num_inst = ctx.num_inst;
        }
    }
}
