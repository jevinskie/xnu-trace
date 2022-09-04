#include "common.h"

#include <CoreSymbolication/CoreSymbolication.h>

std::vector<sym_info> get_symbols(task_t target_task) {
    const auto cs = CSSymbolicatorCreateWithTask(target_task);
    assert(!CSIsNull(cs));
    __block std::vector<sym_info> res;
    CSSymbolicatorForeachSymbolAtTime(cs, kCSNow, ^(CSSymbolRef sym) {
        const auto rng         = CSSymbolGetRange(sym);
        const auto name_cstr   = CSSymbolGetMangledName(sym);
        const std::string name = name_cstr ? name_cstr : "n/a";
        const auto sym_owner   = CSSymbolGetSymbolOwner(sym);
        assert(!CSIsNull(sym_owner));
        const auto sym_owner_path_cstr   = CSSymbolOwnerGetPath(sym_owner);
        const std::string sym_owner_path = sym_owner_path_cstr ? sym_owner_path_cstr : "n/a";
        const auto sym_owner_name_cstr   = CSSymbolOwnerGetName(sym_owner);
        const std::string sym_owner_name = sym_owner_name_cstr ? sym_owner_name_cstr : "n/a";
        res.emplace_back(sym_info{.base     = rng.location,
                                  .size     = rng.length,
                                  .name     = name,
                                  .img_path = sym_owner_path,
                                  .img_name = sym_owner_name});
        return 0;
    });

    std::sort(res.begin(), res.end());

    return res;
}

std::vector<sym_info> get_symbols_in_intervals(const std::vector<sym_info> &syms,
                                               const interval_tree_t<uint64_t> &intervals) {
    std::vector<sym_info> res;
    const auto missing = intervals.cend();
    for (const auto &sym : syms) {
        if (intervals.overlap_find({sym.base, sym.base + sym.size}) != missing) {
            res.emplace_back(sym);
        }
    }
    std::sort(res.begin(), res.end());
    return res;
}

Symbols::Symbols(task_t target_task) : m_target_task{target_task} {
    reset();
}

Symbols::Symbols(const log_sym *sym_buf, uint64_t num_syms) {
#if 0
    for (uint64_t i = 0; i < num_syms; ++i) {
        const char *name_ptr = (const char *)(sym_buf + 1);
        std::string path{path_ptr, region_buf->path_len};
        image_info img_info{.base = region_buf->base, .size = region_buf->size, .path = path};
        memcpy(img_info.uuid, region_buf->uuid, sizeof(img_info.uuid));
        m_regions.emplace_back(img_info);
        region_buf =
            (log_region *)((uint8_t *)region_buf + sizeof(*region_buf) + region_buf->path_len);
    }
    std::sort(m_syms.begin(), m_syms.end());
#endif
}

void Symbols::reset() {
    assert(m_target_task);
    mach_check(task_suspend(m_target_task), "symbols reset suspend");
    m_syms = get_symbols(m_target_task);
    std::sort(m_syms.begin(), m_syms.end());
    mach_check(task_resume(m_target_task), "symbols reset resume");
}

sym_info Symbols::lookup(uint64_t addr) const {
    for (const auto &sym : m_syms) {
        if (sym.base <= addr && addr <= sym.base + sym.size) {
            return sym;
        }
    }
    assert(!"no symbol found");
}

const std::vector<sym_info> &Symbols::syms() const {
    return m_syms;
}
