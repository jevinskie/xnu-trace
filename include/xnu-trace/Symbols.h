#pragma once

#include "common.h"

struct sym_info {
    uint64_t base;
    uint64_t size;
    std::string name;
    std::filesystem::path path;
    auto operator<=>(const sym_info &rhs) const {
        return base <=> rhs.base;
    }
};

XNUTRACE_EXPORT std::vector<sym_info> get_symbols(task_t target_task);

XNUTRACE_EXPORT std::vector<sym_info>
get_symbols_in_intervals(const std::vector<sym_info> &syms,
                         const lib_interval_tree::interval_tree_t<uint64_t> &intervals);

class XNUTRACE_EXPORT Symbols {
public:
    Symbols(task_t target_task);
    Symbols(const log_sym *sym_buf, uint64_t num_syms);
    void reset();
    const std::vector<sym_info> &syms() const;
    const sym_info *lookup(uint64_t addr) const;

private:
    const task_t m_target_task{};
    std::vector<sym_info> m_syms;
};
