#include "common.h"

#include <set>

std::vector<region> get_vm_regions(task_t target_task) {
    std::vector<region> res;
    vm_address_t addr = 0;
    kern_return_t kr;
    natural_t depth = 0;
    while (true) {
        vm_size_t sz{};
        vm_region_submap_info_64 info{};
        mach_msg_type_number_t cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
        kr = vm_region_recurse_64(target_task, &addr, &sz, &depth, (vm_region_recurse_info_t)&info,
                                  &cnt);
        if (kr != KERN_SUCCESS) {
            if (kr != KERN_INVALID_ADDRESS) {
                fmt::print("Error: '{:s}' retval: {:d} description: '{:s}'\n", "get_vm_regions", kr,
                           mach_error_string(kr));
            }
            break;
        }
#if 0
        if (info.protection & 1 && sz && !info.is_submap) {
            const auto buf = read_target(target_task, addr, 128);
            hexdump(buf.data(), buf.size());
        }
#endif
        res.emplace_back(region{.base   = addr,
                                .size   = sz,
                                .depth  = depth,
                                .prot   = info.protection,
                                .tag    = info.user_tag,
                                .submap = !!info.is_submap});
        if (info.is_submap) {
            depth += 1;
            continue;
        }
        addr += sz;
    }

    std::sort(res.begin(), res.end());

#if 0
    for (const auto &map : res) {
        std::string l;
        std::fill_n(std::back_inserter(l), map.depth, '\t');
        l += fmt::format("{:#018x}-{:#018x} {:s} {:#x} {:#04x}", map.base, map.base + map.size,
                         prot_to_str(map.prot), map.size, map.tag);
        fmt::print("{:s}\n", l);
    }
#endif

    return res;
}

VMRegions::VMRegions(task_t target_task) : m_target_task{target_task} {
    reset();
}

void VMRegions::reset() {
    if (m_target_task != mach_task_self()) {
        mach_check(task_suspend(m_target_task), "region reset suspend");
    }
    m_all_regions = get_vm_regions(m_target_task);
    if (m_target_task != mach_task_self()) {
        mach_check(task_resume(m_target_task), "region reset resume");
    }
}
