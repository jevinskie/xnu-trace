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

class log_thread_buf {
public:
    class ctx_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const log_msg;
        using pointer           = const log_msg *;
        using reference         = const log_msg &;

        iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const {
            return *m_ptr;
        }
        pointer operator->() {
            return m_ptr;
        }
        iterator &operator++() {
            m_ptr = (pointer)((uintptr_t)m_ptr + m_ptr->size());
            if (m_ptr->is_sync_frame()) {
                // sync frame guaranteed to be followed by non-sync
                m_ptr = (pointer)((uintptr_t)m_ptr + m_ptr->size());
            }
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        friend bool operator==(const iterator &a, const iterator &b) {
            return a.m_ptr == b.m_ptr;
        };
        friend bool operator!=(const iterator &a, const iterator &b) {
            return a.m_ptr != b.m_ptr;
        };

    private:
        pointer m_ptr{};
    };

    class ctx_iterator : public iterator {
    public:
        ctx_iterator(pointer ptr, const log_arm64_cpu_context *ctx) : iterator(ptr) {
            if (ctx) {
                memcpy(&m_ctx, &ctx, sizeof(m_ctx));
            }
        }
        iterator &operator++() {
            auto &res = iterator::operator++();
            m_ctx.update(*res);
            return res;
        }
        const log_arm64_cpu_context &ctx() const {
            return m_ctx;
        }

    private:
        log_arm64_cpu_context m_ctx;
    };

    class pc_iterator : public iterator {
    public:
        pc_iterator(pointer ptr, uint64_t pc) : iterator(ptr), m_pc{pc} {}
        iterator &operator++() {
            auto &res = iterator::operator++();
            if (res->pc_branched()) {
                m_pc = res->pc();
            } else {
                m_pc += 4;
            }
            return res;
        }
        uint64_t pc() const {
            return m_pc;
        }

    private:
        uint64_t m_pc;
    };

    log_thread_buf() = default;
    log_thread_buf(const std::vector<uint8_t> &&buf, uint64_t num_inst)
        : m_buf{buf}, m_num_inst{num_inst} {};

    uint64_t num_inst() const {
        return m_num_inst;
    }
    uint64_t num_bytes() const {
        return m_buf.size();
    }

    const log_msg &front() const {
        const auto &res = *(log_msg *)m_buf.data();
        assert(res.is_sync_frame());
        return res;
    }

    iterator begin() const {
        return iterator((log_msg *)m_buf.data());
    }
    iterator end() const {
        return iterator((log_msg *)(m_buf.data() + m_buf.size()));
    }

    ctx_iterator ctx_begin() const {
        return ctx_iterator((log_msg *)m_buf.data(), front().sync_ctx());
    }
    ctx_iterator ctx_end() const {
        return ctx_iterator((log_msg *)(m_buf.data() + m_buf.size()), nullptr);
    }

    pc_iterator pcs_begin() const {
        return pc_iterator((log_msg *)m_buf.data(), front().sync_ctx()->pc);
    }
    pc_iterator pcs_end() const {
        return pc_iterator((log_msg *)(m_buf.data() + m_buf.size()), 0);
    }

private:
    std::vector<uint8_t> m_buf;
    uint64_t m_num_inst{};
};

static_assert(std::is_move_constructible_v<log_thread_buf>,
              "log_thread_buf not move constructable");
static_assert(std::is_move_assignable_v<log_thread_buf>, "log_thread_buf not move assignable");

XNUTRACE_EXPORT std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs);
XNUTRACE_EXPORT std::vector<bb_t> extract_bbs_from_pc_trace(const log_thread_buf &thread_buf);
XNUTRACE_EXPORT std::vector<uint64_t> extract_pcs_from_trace(const log_thread_buf &thread_buf);

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
    const std::map<uint32_t, log_thread_buf> &parsed_logs() const;
    static constexpr uint32_t sync_every = 1024 * 1024; // 1 MB, overhead 0.09% per MB

private:
    struct thread_ctx {
        std::vector<uint8_t> log_buf;
        std::unique_ptr<CompressedFile<log_thread_hdr>> log_stream;
        XNUTRACE_ALIGNED(16) log_arm64_cpu_context last_cpu_ctx;
        uint64_t num_inst{};
        uint32_t sz_since_last_sync{sync_every + 1};
        XNUTRACE_INLINE void write_log_msg(const log_arm64_cpu_context *ctx);
        XNUTRACE_INLINE void write_log_msg(uint64_t pc);
        void write_sync();
    };
    uint64_t m_num_inst{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<Symbols> m_symbols;
    std::map<uint32_t, log_thread_buf> m_parsed_logs;
    std::filesystem::path m_log_dir_path;
    int m_compression_level{};
    bool m_stream{};
    mph_map<uint32_t, thread_ctx> m_thread_ctxs;
};
