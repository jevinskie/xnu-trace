#pragma once

#include "common.h"

#include <mach/mach_types.h>
#include <mach/vm_prot.h>

struct region {
    uint64_t base;
    uint64_t size;
    uint64_t depth;
    std::optional<std::filesystem::path> path;
    vm_prot_t prot;
    uint32_t tag;
    bool submap;
    auto operator<=>(const region &rhs) const {
        return base <=> rhs.base;
    }
};

XNUTRACE_EXPORT std::vector<region> get_vm_regions(task_t target_task);

class XNUTRACE_EXPORT VMRegions {
public:
    VMRegions(task_t target_task);

    void reset();

private:
    const task_t m_target_task;
    std::vector<region> m_all_regions;
};
