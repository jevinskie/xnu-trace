#pragma once

#include "common.h"

struct bb_t {
    uint64_t pc;
    uint32_t sz;
} __attribute__((packed));

XNUTRACE_EXPORT std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs);
XNUTRACE_EXPORT std::vector<uint64_t>
extract_pcs_from_trace(const std::span<const log_msg_hdr> &msgs);

class XNUTRACE_EXPORT TraceLog {
public:
    TraceLog(const std::string &log_dir_path, int compression_level, bool stream);
    TraceLog(const std::string &log_dir_path);
    XNUTRACE_INLINE void log(thread_t thread, uint64_t pc);
    void write(const MachORegions &macho_regions, const Symbols *symbols = nullptr);
    uint64_t num_inst() const;
    size_t num_bytes() const;
    const MachORegions &macho_regions() const;
    const Symbols &symbols() const;
    const std::map<uint32_t, std::vector<log_msg_hdr>> &parsed_logs() const;

private:
    uint64_t m_num_inst{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<Symbols> m_symbols;
    std::map<uint32_t, std::vector<uint8_t>> m_log_bufs;
    std::map<uint32_t, std::unique_ptr<CompressedFile<log_thread_hdr>>> m_log_streams;
    std::map<uint32_t, std::vector<log_msg_hdr>> m_parsed_logs;
    std::filesystem::path m_log_dir_path;
    int m_compression_level{};
    bool m_stream{};
    absl::flat_hash_map<uint32_t, uint64_t> m_thread_num_inst;
};
