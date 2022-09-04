#include "common.h"

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
