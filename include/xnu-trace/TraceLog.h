#pragma once

#include "common.h"

#include "CompressedFile.h"
#include "MachORegions.h"
#include "MinimalPerfectHash.h"
#include "Symbols.h"
#include "log_structs.h"
#include "mach.h"

#include <memory>
#include <span>
#include <vector>

#include <mach/mach_types.h>

#include <absl/container/flat_hash_map.h>

struct bb_t {
    uint64_t pc;
    uint32_t sz;
} __attribute__((packed));

XNUTRACE_EXPORT std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs);
XNUTRACE_EXPORT std::vector<uint64_t> extract_pcs_from_trace(const log_msg *msgs_begin,
                                                             const log_msg *msgs_end);
XNUTRACE_EXPORT std::vector<uint64_t> extract_pcs_from_trace(const std::span<const uint8_t> msgs);
XNUTRACE_EXPORT std::vector<uint64_t> extract_pcs_from_trace(const std::vector<uint8_t> &msgs);

class XNUTRACE_EXPORT TraceLog {
public:
    TraceLog(const std::string &log_dir_path, int compression_level, bool stream);
    TraceLog(const std::string &log_dir_path);
    XNUTRACE_INLINE void log(thread_t thread, uint64_t pc);
    XNUTRACE_INLINE void log(thread_t thread, const log_arm64_cpu_context *context);
    void write(const MachORegions &macho_regions, const Symbols *symbols = nullptr);
    uint64_t num_inst() const;
    size_t num_bytes() const;
    const MachORegions &macho_regions() const;
    const Symbols &symbols() const;
    const std::map<uint32_t, std::vector<uint8_t>> &parsed_logs() const;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
    XNUTRACE_INLINE static size_t build_frida_log_msg(const log_arm64_cpu_context *ctx,
                                                      const log_arm64_cpu_context *last_ctx,
                                                      uint8_t XNUTRACE_ALIGNED(16)
                                                          msg_buf[log_msg::max_size]);
#pragma clang diagnostic pop

private:
    struct thread_ctx {
        std::vector<uint8_t> log_buf;
        std::unique_ptr<CompressedFile<log_thread_hdr>> log_stream;
        XNUTRACE_ALIGNED(16) log_arm64_cpu_context last_cpu_ctx;
        uint64_t last_pc;
        uint64_t num_inst;
    };

    uint64_t m_num_inst{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<Symbols> m_symbols;
    std::map<uint32_t, std::vector<uint8_t>> m_parsed_logs;
    std::filesystem::path m_log_dir_path;
    int m_compression_level{};
    bool m_stream{};
    mph_map<uint32_t, thread_ctx> m_thread_ctxs;
};
