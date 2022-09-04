#include "common.h"

MachORegions::MachORegions(task_t target_task) : m_target_task{target_task} {
    reset();
}

MachORegions::MachORegions(const log_region *region_buf, uint64_t num_regions) {
    for (uint64_t i = 0; i < num_regions; ++i) {
        const char *path_ptr = (const char *)(region_buf + 1);
        std::string path{path_ptr, region_buf->path_len};
        image_info img_info{.base = region_buf->base, .size = region_buf->size, .path = path};
        memcpy(img_info.uuid, region_buf->uuid, sizeof(img_info.uuid));
        m_regions.emplace_back(img_info);
        region_buf =
            (log_region *)((uint8_t *)region_buf + sizeof(*region_buf) + region_buf->path_len);
    }
    std::sort(m_regions.begin(), m_regions.end());
}

void MachORegions::reset() {
    assert(m_target_task);
    mach_check(task_suspend(m_target_task), "region reset suspend");
    m_regions = get_dyld_image_infos(m_target_task);
    std::vector<uint64_t> region_bases;
    for (const auto &region : m_regions) {
        region_bases.emplace_back(region.base);
    }
    int num_jit_regions = 0;
    for (const auto &vm_region : get_vm_regions(m_target_task)) {
        if (!(vm_region.prot & VM_PROT_EXECUTE)) {
            continue;
        }
        if (!(vm_region.prot & VM_PROT_READ)) {
            fmt::print("found XO page at {:#018x}\n", vm_region.base);
        }
        if (std::find(region_bases.cbegin(), region_bases.cend(), vm_region.base) !=
            region_bases.cend()) {
            continue;
        }
        if (vm_region.tag != 0xFF) {
            continue;
        }
        m_regions.emplace_back(image_info{
            .base = vm_region.base,
            .size = vm_region.size,
            .path = fmt::format("/tmp/pid-{:d}-jit-region-{:d}-tag-{:02x}",
                                pid_for_task(m_target_task), num_jit_regions, vm_region.tag),
            .uuid = {}});
        ++num_jit_regions;
    }
    std::sort(m_regions.begin(), m_regions.end());
    mach_check(task_resume(m_target_task), "region reset resume");
}

image_info MachORegions::lookup(uint64_t addr) const {
    for (const auto &img_info : m_regions) {
        if (img_info.base <= addr && addr <= img_info.base + img_info.size) {
            return img_info;
        }
    }
    assert(!"no region found");
}

std::pair<image_info, size_t> MachORegions::lookup_idx(uint64_t addr) const {
    size_t idx = 0;
    for (const auto &img_info : m_regions) {
        if (img_info.base <= addr && addr <= img_info.base + img_info.size) {
            return std::make_pair(img_info, idx);
        }
        ++idx;
    }
    assert(!"no region found");
}

const std::vector<image_info> &MachORegions::regions() const {
    return m_regions;
}
