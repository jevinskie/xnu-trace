#include "common.h"

std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       uint64_t macho_hdr_addr) {
    std::vector<segment_command_64> segs;
    const auto hdr_buf = read_target(target_task, macho_hdr_addr, sizeof(mach_header_64));
    const auto hdr     = (mach_header_64 *)hdr_buf.data();
    const auto cmd_buf =
        read_target(target_task, macho_hdr_addr + sizeof(mach_header_64), hdr->sizeofcmds);
    const auto end_of_lc = (load_command *)(cmd_buf.data() + hdr->sizeofcmds);
    for (auto lc = (load_command *)cmd_buf.data(); lc < end_of_lc;
         lc      = (load_command *)((uint8_t *)lc + lc->cmdsize)) {
        if (lc->cmd != LC_SEGMENT_64) {
            continue;
        }
        const auto seg = (segment_command_64 *)lc;
        if (!strncmp(seg->segname, "__PAGEZERO", sizeof(seg->segname))) {
            continue;
        }
        segs.emplace_back(*seg);
    }
    return segs;
}

std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       const mach_header_64 *macho_hdr) {
    return read_macho_segs_target(target_task, (uint64_t)macho_hdr);
}

uint64_t get_text_size(const std::vector<segment_command_64> &segments) {
    uint32_t num_exec_segs = 0;
    uint64_t exec_file_sz;
    for (const auto &seg : segments) {
        if (seg.maxprot & VM_PROT_EXECUTE) {
            ++num_exec_segs;
            exec_file_sz = seg.vmsize;
        }
    }
    assert(num_exec_segs == 1);
    return exec_file_sz;
}
