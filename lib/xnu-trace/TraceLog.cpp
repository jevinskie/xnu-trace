#include "xnu-trace/TraceLog.h"
#include "common-internal.h"

#include "xnu-trace/Signpost.h"
#include "xnu-trace/ThreadPool.h"

#include <absl/container/flat_hash_set.h>
#include <arm_neon.h>
#include <interval-tree/interval_tree.hpp>
#undef G_DISABLE_ASSERT
#include <frida-gum.h>
#include <simde/arm/neon.h>

using namespace lib_interval_tree;

// clang-format off
#define gum_arm64_cpu_context_uninit_buf { \
    0xbe55c8198f0f369cULL, 0x9d62073c5f9306bcULL, 0x78ec7b54f67c1b8eULL, 0x69a601a53899d636ULL, \
    0x9f532d6b355337a4ULL, 0x039b42a2e30911f9ULL, 0x71fd76875c6829d9ULL, 0x3e515c53b8fdc5c1ULL, \
    0xe53f46ce3d3ca1abULL, 0x6f2e3ba39601e9c7ULL, 0x13fb815207cb98d4ULL, 0x7356c99a7957a796ULL, \
    0x0a55c9d09c2041a2ULL, 0x256295b3cc9b9252ULL, 0x7745a3e6c12279ddULL, 0x08aa54af6a2a8ed2ULL, \
    0x0f5fe8a05d75ce0fULL, 0x3db05b460fc342b8ULL, 0x20e38abc3855e841ULL, 0x62da617307295f04ULL, \
    0xfb7587425bd4ecccULL, 0x40b0ef83ba087947ULL, 0x57c81794d4220171ULL, 0xb53200a90b06e7d5ULL, \
    0x373d4ffa878a0588ULL, 0x6341c84fed9be50bULL, 0x0843d0a95da79aa2ULL, 0x0daa65eaa4014189ULL, \
    0x6b5269fe8638712cULL, 0xa90f7de2b6fdab6aULL, 0xe4f510ddf78868f7ULL, 0xb769b87d1aca97a6ULL, \
    0x5895b94e85dafb07ULL, 0xdada4cbfb0ec5c10ULL, 0xd334dbcf438373f8ULL, 0xc47d59806e5b6d0dULL, \
    0x58f073f53d1b332eULL, 0x4b5094adc8729311ULL, 0x29ceee7984957fb0ULL, 0x2395955bdb32dc0bULL, \
    0x7b150373015a866eULL, 0x379934e4b4bb352fULL, 0xeca9e4bc7abeb8b9ULL, 0xb2130a3f96ac56d1ULL, \
    0x4ca55a7a807e3893ULL, 0x94de93cf272a62bfULL, 0xe6b8de18eed9a429ULL, 0x47f37f61e690e4f6ULL, \
    0xfca7cd9c7eade516ULL, 0xf4a1184d718e8eb5ULL, 0x8609cf6e61b85440ULL, 0xb73b2fe948a3b78cULL, \
    0xc2a45e149b7bec11ULL, 0xeef0d2ae475344baULL, 0x2d5dd067009c442bULL, 0x38007f192e450df7ULL, \
    0xb2f4d8a81cb95ea8ULL, 0xa187b38923559584ULL, 0xd8d41dd2e61793e8ULL, 0xc9dc070c8edee3baULL, \
    0xbc03598c63bffef9ULL, 0x4d91a42e54e725baULL, 0x5ba72fc61d29b1b7ULL, 0x5cf4912e1d3a663cULL, \
    0x3998f3ebca0a2cfbULL, 0xd9a18187acc75110ULL, 0x99c8b0106c91cca5ULL, 0x46b118ee342e0471ULL, \
    0x5b60908d89dad17aULL, 0x1f366f9104245eb7ULL, 0xc2c26a66538c51feULL, 0xa601ba0a8cb728baULL, \
    0xd3d9b5378e049d28ULL, 0xca8f40fa3d909ce7ULL, 0xa5677caf64cb0d27ULL, 0x987ad9f2c6869358ULL, \
    0x056831c9c7faa69aULL, 0x99d1562f189695d5ULL, 0x2d66e75217fb2071ULL, 0x51765f27f147b397ULL, \
    0x37a5c01219e4ba27ULL, 0x76a5c7a60a3c383eULL, 0x89f2a3aada6dbe7dULL, 0x231f774987480c64ULL, \
    0x258c031b009d084fULL, 0xa91b22230a8270a0ULL, 0x2773a83768a94422ULL, 0x15b6ae70f4c08200ULL, \
    0x938f8e972a54aebaULL, 0x253ae11b3327337cULL, 0x9a1b12e22ee53d63ULL, 0x50e3706cc7028fe6ULL, \
    0xe0314592d8161c5eULL, 0xf5665fabbe3ec55bULL, 0xbc43fdff186e7fe0ULL, 0xb5b4062f669fb2ecULL, \
    0x3f73fe8d82a81652ULL, 0xc4b8f87e6e0935daULL}
// clang-format on

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
    pcs.resize(msgs.size());
    size_t i = 0;
    for (const auto &msg : msgs) {
        // FIXME
        // pcs[i++] = msg.pc;
    }
    return pcs;
}

TraceLog::thread_ctx_map::thread_ctx_map(const std::filesystem::path &log_dir_path)
    : m_log_dir_path{log_dir_path} {}

TraceLog::thread_ctx_map::thread_ctx_map(const std::filesystem::path &log_dir_path,
                                         int compression_level, bool stream)
    : m_log_dir_path{log_dir_path}, m_compression_level{compression_level}, m_stream{stream} {}

TraceLog::thread_ctx &TraceLog::thread_ctx_map::operator[](uint32_t key) {
    if (!contains(key)) {
        std::unique_ptr<CompressedFile<log_thread_hdr>> log_stream;
        if (m_stream) {
            const log_thread_hdr thread_hdr{.thread_id = key};
            log_stream = std::make_unique<CompressedFile<log_thread_hdr>>(
                m_log_dir_path / fmt::format("thread-{:d}.bin", key), false, &thread_hdr,
                m_compression_level);
        }
        try_emplace(key, thread_ctx{.log_stream   = std::move(log_stream),
                                    .last_cpu_ctx = gum_arm64_cpu_context_uninit_buf});
    }
    return mph_map<uint32_t, thread_ctx>::operator[](key);
}

TraceLog::TraceLog(const std::string &log_dir_path, int compression_level, bool stream)
    : m_log_dir_path{log_dir_path}, m_compression_level{compression_level}, m_stream{stream},
      m_thread_ctxs{m_log_dir_path, compression_level, stream} {
    fs::create_directory(m_log_dir_path);
    for (const auto &dirent : std::filesystem::directory_iterator{m_log_dir_path}) {
        if (!dirent.path().filename().string().starts_with("macho-region-")) {
            fs::remove(dirent.path());
        }
    }
}

TraceLog::TraceLog(const std::string &log_dir_path)
    : m_log_dir_path{log_dir_path}, m_thread_ctxs{m_log_dir_path} {
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

    std::vector<std::pair<uint32_t, std::vector<log_msg_hdr>>> parsed_logs_vec(thread_paths.size());
    for (size_t i = 0; i < thread_paths.size(); ++i) {
        xnutrace_pool.push_task([&, i] {
            const auto path = thread_paths[i];
            Signpost thread_read_sp("TraceLogThreads",
                                    fmt::format("{:s} read", path.filename().string()));
            thread_read_sp.start();
            CompressedFile<log_thread_hdr> thread_fh{path, true};
            const auto thread_buf = thread_fh.read();
            const auto thread_hdr = thread_fh.header();
            thread_read_sp.end();

            Signpost thread_parse_sp("TraceLogThreads",
                                     fmt::format("{:s} parse", path.filename().string()));
            thread_parse_sp.start();
            std::vector<log_msg_hdr> thread_log;
            auto inst_hdr           = (log_msg_hdr *)thread_buf.data();
            const auto inst_hdr_end = (log_msg_hdr *)(thread_buf.data() + thread_buf.size());
            thread_log.resize(thread_hdr.num_inst);
            size_t idx = 0;
            while (inst_hdr < inst_hdr_end) {
                thread_log[idx++] = *inst_hdr;
                ++inst_hdr;
            }
            parsed_logs_vec[i] = {thread_hdr.thread_id, std::move(thread_log)};
            thread_parse_sp.end();
        });
    }
    xnutrace_pool.wait_for_tasks();

    for (const auto &[thread_id, log] : parsed_logs_vec) {
        m_num_inst += log.size();
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

const std::map<uint32_t, std::vector<log_msg_hdr>> &TraceLog::parsed_logs() const {
    return m_parsed_logs;
}

static uint64x2_t interleave_uint64x2_with_zeros_16bit(uint64x2_t input) {
    uint64x2_t word = input;
    word            = (word ^ (word << 8)) & 0x00ff00ff00ff00ff;
    word            = (word ^ (word << 4)) & 0x0f0f0f0f0f0f0f0f;
    word            = (word ^ (word << 2)) & 0x3333333333333333;
    word            = (word ^ (word << 1)) & 0x5555555555555555;
    return word;
}

size_t TraceLog::build_frida_log_msg(const void *context, const void *last_context,
                                     uint8_t XNUTRACE_ALIGNED(16) msg_buf[rpc_changed_max_sz]) {
    assert(((uintptr_t)context & 0b1111) == 0 && "cpu context not 16 byte aligned");
    const auto ctx      = (GumCpuContext *)XNUTRACE_ASSUME_ALIGNED(context, 16);
    const auto last_ctx = (GumCpuContext *)XNUTRACE_ASSUME_ALIGNED(last_context, 16);

    auto *msg_hdr        = (log_msg_hdr *)msg_buf;
    uint8_t *buf_ptr     = msg_buf + sizeof(log_msg_hdr);
    uint32_t gpr_changed = 0;

    const auto last_pc_sp = *(uint64x2_t *)&last_ctx->pc;
    const auto pc_sp      = *(uint64x2_t *)&ctx->pc;
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

    const auto last_x2 = (uint64x2_t *)&last_ctx->x[0];
    const auto x2      = (uint64x2_t *)&ctx->x[0];
    uint64x2_t vx_diff = {};

    for (int i = 0; i < 32 / 2; ++i) {
        auto diff_mask = x2[i] != last_x2[i];
        diff_mask &= 1;
        diff_mask <<= i;
        vx_diff |= diff_mask;
    }

    uint32_t x_diff = 0;

    msg_hdr->gpr_changed = gpr_changed;
    msg_hdr->vec_changed = vx_diff[0] | vx_diff[1];

    return 0;
}

void TraceLog::log(thread_t thread, const void *context) {
    auto &tctx = m_thread_ctxs[thread];
    uint8_t __attribute__((uninitialized, aligned(16))) msg_buf[rpc_changed_max_sz];
    const auto msg_sz = build_frida_log_msg(context, (void *)tctx.last_cpu_ctx, msg_buf);

    if (!m_stream) {
        std::copy(msg_buf, msg_buf + msg_sz, std::back_inserter(tctx.log_buf));
    } else {
        tctx.log_stream->write(msg_buf, msg_sz);
    }
    memcpy(tctx.last_cpu_ctx, context, sizeof(tctx.last_cpu_ctx));
    ++tctx.num_inst;
    ++m_num_inst;
}

void TraceLog::log(thread_t thread, uint64_t pc) {
    auto &ctx          = m_thread_ctxs[thread];
    const auto last_pc = ctx.last_pc;

    uint8_t __attribute__((uninitialized, aligned(16))) msg_buf[rpc_changed_max_sz];
    auto *msg_hdr        = (log_msg_hdr *)msg_buf;
    uint8_t *buf_ptr     = msg_buf + sizeof(log_msg_hdr);
    uint32_t gpr_changed = 0;

    if (last_pc + 4 != pc) {
        gpr_changed          = rpc_set_pc_branched(gpr_changed);
        *(uint64_t *)buf_ptr = pc;
        buf_ptr += sizeof(uint64_t);
    }

    msg_hdr->gpr_changed = gpr_changed;
    msg_hdr->vec_changed = 0;

    if (!m_stream) {
        std::copy(msg_buf, buf_ptr, std::back_inserter(ctx.log_buf));
    } else {
        ctx.log_stream->write(msg_buf, buf_ptr - msg_buf);
    }
    ctx.last_pc = pc;
    ++ctx.num_inst;
    ++m_num_inst;
}

void TraceLog::write(const MachORegions &macho_regions, const Symbols *symbols) {
    interval_tree_t<uint64_t> pc_intervals;

    if (!m_stream) {
        absl::flat_hash_set<uint64_t> pcs;
        for (const auto &[tid, ctx] : m_thread_ctxs) {
            for (const auto pc :
                 extract_pcs_from_trace({(log_msg_hdr *)ctx.log_buf.data(),
                                         ctx.log_buf.size() / sizeof(log_msg_hdr)})) {
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
